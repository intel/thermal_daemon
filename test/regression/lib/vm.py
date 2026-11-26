"""Host side: build a scenario's ACPI table and run it in a virtme-ng VM."""

import json
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gddv  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))

DEFAULT_CPU_MODEL = 0xaa   # family 6: Meteor Lake L

# Adaptive performance policy UUID, reported by IETM.IDSP
ADAPTIVE_UUID = "63be270f-1c11-48fd-a6f7-3af253ff3e2d"

ASL_TEMPLATE = """\
DefinitionBlock ("", "SSDT", 2, "THDTST", "IETM", 0x1)
{
    Scope (\\_SB)
    {
        Device (IETM)
        {
            Name (_HID, "%(hid)s")
            Name (_UID, Zero)
            Name (IMKV, 0xFFFFFFFF)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Method (IDSP, 0, NotSerialized)
            {
                Return (Package () { %(uuids)s })
            }
            Method (GDDV, 0, NotSerialized)
            {
                Return (Package () { Buffer (%(len)d) { %(bytes)s } })
            }
            Method (IMOK, 1, NotSerialized)
            {
                IMKV = Arg0
                // Shows in dmesg with acpi debug_level 0x2 (guest.py)
                Debug = Concatenate ("THDTST IMOK ", ToDecimalString (Arg0))
                Return (Arg0)
            }
%(odvp)s        }
    }
}
"""


def build_acpi_table(platform, workdir):
    """Write ietm.aml for the platform's GDDV, return its path or None."""
    if platform.get("gddv") is None:
        return None

    vault = gddv.encode(platform["gddv"])
    with open(os.path.join(workdir, "data_vault.bin"), "wb") as f:
        f.write(vault)

    uuids = platform.get("idsp", [ADAPTIVE_UUID])
    asl_vars = {
        "hid": platform.get("ietm_hid", "INTC10D4"),
        "uuids": ", ".join('ToUUID ("%s")' % u for u in uuids),
        "len": len(vault),
        "bytes": ",".join("0x%02X" % b for b in vault),
        "odvp": "",
    }
    if platform.get("odvp") is not None:
        # OEM variables, exposed by int3400 as odvp0..N (Oem0..5 conditions)
        asl_vars["odvp"] = (
            "            Method (ODVP, 0, NotSerialized)\n"
            "            {\n"
            "                Return (Package () { %s })\n"
            "            }\n" % ", ".join("0x%X" % v for v in platform["odvp"]))
    asl = ASL_TEMPLATE % asl_vars
    asl_path = os.path.join(workdir, "ietm.asl")
    with open(asl_path, "w") as f:
        f.write(asl)

    # Note: iasl -q produces a table with an "XXXX" signature
    subprocess.run(["iasl", asl_path], cwd=workdir, check=True,
                   capture_output=True, text=True)
    return os.path.join(workdir, "ietm.aml")


def vm_kernel_image(release):
    """Kernel image of an installed kernel: /boot/vmlinuz-<release>
    (Fedora, Ubuntu) or /lib/modules/<release>/vmlinuz (Arch, CachyOS)."""
    for path in ("/boot/vmlinuz-" + release,
                 "/lib/modules/%s/vmlinuz" % release):
        if os.path.exists(path):
            return path
    return "/boot/vmlinuz-" + release


def thermald_env():
    """Pass THERMALD_BIN (test another build) through to the guest."""
    binary = os.environ.get("THERMALD_BIN")
    if not binary:
        return ""
    return "THERMALD_BIN=%s " % os.path.abspath(binary)


def run_scenario(scenario_path, outdir, platform, timeout=600):
    """Boot a VM, run the scenario in it, return the result dict."""
    shutil.rmtree(outdir, ignore_errors=True)
    os.makedirs(outdir)

    aml = build_acpi_table(platform, outdir)

    # The guest uses this instead of the scenario's PLATFORM, so platforms
    # built on the host (real vaults) work too. The vault itself is in
    # the ACPI table.
    guest_platform = {k: v for k, v in platform.items() if k != "gddv"}
    guest_platform["gddv"] = True if platform.get("gddv") is not None else None
    with open(os.path.join(outdir, "platform.json"), "w") as f:
        json.dump(guest_platform, f, indent=1)

    # THD_VM_KERNEL: boot that installed kernel instead of the running one
    kernel = os.environ.get("THD_VM_KERNEL")
    run = ["--run", vm_kernel_image(kernel)] if kernel else ["--run"]
    cmd = ["vng"] + run + ["--disable-microvm",
           "--cpus", "2", "--memory", "1G",
           "--overlay-rwdir=/etc", "--rwdir=" + outdir,
           "--exec", "%spython3 %s %s %s %s" % (
               thermald_env(), os.path.join(HERE, "guest.py"), REPO,
               os.path.abspath(scenario_path), outdir)]
    qemu_opts = []
    if aml:
        qemu_opts.append("-acpitable file=" + aml)
    if platform.get("cpu"):
        qemu_opts.append("-cpu " + platform["cpu"])
    else:
        # Pin the CPU model thermald sees, so results don't depend on the
        # host. Default: Meteor Lake L, supported in both adaptive and
        # non-adaptive mode. Set "cpu_model" for e.g. adaptive-only CPUs.
        qemu_opts.append("-cpu host,family=6,model=%d"
                         % platform.get("cpu_model", DEFAULT_CPU_MODEL))
    if qemu_opts:
        cmd.append("--qemu-opts=" + " ".join(qemu_opts))

    with open(os.path.join(outdir, "vm.log"), "w") as log:
        try:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT,
                           stdin=subprocess.DEVNULL, timeout=timeout)
        except subprocess.TimeoutExpired:
            return {"passed": False, "error": "VM timed out after %ds" % timeout}

    result_path = os.path.join(outdir, "result.json")
    if not os.path.exists(result_path):
        with open(os.path.join(outdir, "vm.log"), errors="replace") as f:
            tail = f.read()[-3000:]
        return {"passed": False, "error": "no result from VM:\n" + tail}

    with open(result_path) as f:
        return json.load(f)
