# Testing thermald

This is the one guide to testing thermald: what to run, how to set it
up, what each test checks, and how to add tests.

## Quick start

    ./autogen.sh prefix=/ && make -j$(nproc)    # once
    test/run_all.sh                              # ~7 min, no root needed

It builds what it needs, runs the parser unit tests, a short run of each
fuzzer and the VM regression scenarios, and ends with one line per layer:

    unit: PASS
    fuzz: PASS
    vm: PASS

Install the packages listed under [Packages](#packages) first. Add
`--vault-dir DIR` to also test real data vaults (see
[Real data vaults](#real-data-vaults)).

`run_all.sh` does **not** run the hardware smoke test or the manual
tests: they need root and a real, supported machine, and they change
its real power limits while they run. Run the hardware smoke test
separately, on the machine you want to check, with the thermald service
stopped:

    sudo systemctl stop thermald
    sudo test/hw/hw_smoke.py --seconds 120 --load 60
    sudo systemctl start thermald

It runs thermald with `--ignore-cpuid-check`, so pre-production CPU
models and Lenovo ThinkPads with DYTC are tested too, and keeps
thermald's log (`--loglevel=info`) and a plot of temperature and power
over the run. It prints PASS, FAIL or SKIP and
the paths of thermald's log and its report, both in
`/tmp/thermald-hw/<host>-<time>/`. See
[Hardware smoke test](#hardware-smoke-test-hwhw_smokepy) for what it
checks and [Manual tests](#manual-tests) for the older hand-run tests.

## What to run for a change

| You changed | Run |
|---|---|
| anything, before sending a patch | `test/run_all.sh` |
| the data vault parser (`src/thd_gddv.cpp`, LZMA) | `pytest test/unit`, then `test/fuzz/run.sh gddv` for a few minutes |
| the XML parsers (`src/thd_parse.cpp`, `thd_features_parse.cpp`, `thd_cdev_order_parser.cpp`) | `test/fuzz/run.sh xml` for a few minutes, then `test/regression/run.sh -k xml` |
| the D-Bus interface (`src/thd_dbus_interface.cpp`, `user_*` in `thd_engine.cpp`) | `test/regression/run.sh -k dbus` |
| adaptive engine, conditions, targets, PSVT, PPCC, RAPL, cooling device logic | `test/regression/run.sh`, or `-k <scenario>` while iterating |
| XML configuration handling (`thermal-conf.xml`) | `test/regression/run.sh -k xml` |
| something only a real machine shows (sensors, firmware, power limits) | `sudo test/hw/hw_smoke.py` on that machine |

## Layers

| Layer | Directory | Needs | What it covers |
|---|---|---|---|
| Parser unit tests | `unit/` | gcc, pytest | thermald's GDDV (data vault) parser, built with AddressSanitizer and UBSan: round trips of every table type, every truncation of a valid vault, byte mutations, huge lengths |
| Fuzzing | `fuzz/` | clang | libFuzzer with ASan and UBSan on the GDDV parser, the three XML configuration parsers and the integer, floating point and name helpers |
| VM regression | `regression/` | KVM, virtme-ng | the real thermald binary in a VM on a fake platform: PPCC and RAPL limits, condition tables, temperature limits and restore, several zones on one cooling device, XML configs, malformed vaults, random D-Bus calls (to an ASan build), recent fixes |
| Hardware smoke | `hw/` | root, a supported machine | the real thermald binary on a real machine for a minute or two |
| Manual tests | `manual/` | root, a real machine | older tests run by hand, see [Manual tests](#manual-tests) |

## Directory layout

    test/
      README.md        this guide
      run_all.sh       runs unit, fuzz and VM layers
      unit/            parser unit tests (gddv_dump, test_gddv_parser.py)
      fuzz/            libFuzzer targets (gddv, xml, util), seeds, run.sh
      regression/      VM suite: run.sh, scenarios/, configs/, lib/, real/
      hw/              hw_smoke.py, for real machines
      module/          the test kernel module: thermal zones, cooling
                       devices and fake RAPL (used by regression/, manual/)
      manual/
        emulation/     emul_temp driven scripts and their XML configs
        configs/       thermal_conf.xml.N walkthrough (test_cases.txt)
        data_vault/    reproducing another machine's DTT tables, sample
                       vault, gddv_tests/ generator

Debug collection and plotting scripts are not tests; they are in
`tools/` (see [Debug tools](#debug-tools)).

## Packages

Tested on Fedora 44 and Ubuntu 25.10; the Arch Linux / CachyOS
package names are from their official repositories. Install everything
with one
command, or only what the layers you want need (table below).

### Fedora

    sudo dnf install automake autoconf-archive gcc gcc-c++ glib2-devel \
        libxml2-devel gtk-doc upower-devel libevdev-devel \
        python3-pytest libasan libubsan python3-matplotlib \
        clang compiler-rt llvm \
        virtme-ng qemu-system-x86-core acpica-tools python3-dbusmock \
        python3-evdev python3-dbus dbus-daemon kernel-devel-$(uname -r)

### Ubuntu

    sudo apt-get install autoconf autoconf-archive automake g++ \
        gtk-doc-tools libglib2.0-dev libxml2-dev libupower-glib-dev \
        libevdev-dev \
        python3-pytest python3-matplotlib \
        clang libclang-rt-dev llvm \
        virtme-ng qemu-system-x86 acpica-tools python3-dbusmock \
        python3-evdev python3-dbus dbus-daemon linux-headers-$(uname -r)

On Ubuntu the kernel image is readable only by root, and virtme-ng has
to read it to boot the VM:

    sudo chmod 0644 /boot/vmlinuz-$(uname -r)

If your Ubuntu release has no `virtme-ng` package, use
`pip3 install --break-system-packages virtme-ng` (or `pipx install
virtme-ng`). The sanitizer runtimes come with g++, libFuzzer with
`libclang-rt-dev`, `llvm-symbolizer` (function names and lines in
sanitizer reports) with `llvm`.

### Arch Linux and CachyOS

    sudo pacman -S --needed base-devel autoconf-archive glib2 libxml2 \
        gtk-doc upower libevdev \
        python-pytest python-matplotlib \
        clang compiler-rt lld llvm \
        virtme-ng qemu-system-x86 acpica python-dbusmock python-evdev \
        python-dbus dbus

plus the headers of the kernel you run, which have the kernel package's
name with `-headers`: `linux-headers` for Arch's `linux`,
`linux-cachyos-headers` for CachyOS's default `linux-cachyos`, and
`linux-cachyos-bore-headers`, `linux-cachyos-lts-headers` and so on for
the other CachyOS kernels. To find the package of the running kernel:

    pacman -Qqo /usr/lib/modules/$(uname -r)/vmlinuz

All packages are in the official repositories (CachyOS uses the Arch
ones plus its own kernels), none from the AUR. Arch has no separate
development packages: headers come with the libraries. The sanitizer
runtimes come with gcc, libFuzzer with `compiler-rt`, and `iasl` with
`acpica`. The kernel image is `/usr/lib/modules/<release>/vmlinuz` and
already readable.

CachyOS kernels are built with clang (and LTO), so the test kernel
module has to be built with the LLVM toolchain too: that is why `lld`
and `llvm` are in the list. `test/module/Makefile` notices a clang-built
kernel (`CONFIG_CC_IS_CLANG=y`) and passes `LLVM=1` itself.

### What each layer needs

| Layer | Fedora | Ubuntu | Arch Linux / CachyOS |
|---|---|---|---|
| Building thermald (all layers) | `automake autoconf-archive gcc gcc-c++ glib2-devel libxml2-devel gtk-doc upower-devel libevdev-devel` | `autoconf autoconf-archive automake g++ gtk-doc-tools libglib2.0-dev libxml2-dev libupower-glib-dev libevdev-dev` | `base-devel autoconf-archive glib2 libxml2 gtk-doc upower libevdev` |
| Parser unit tests | `python3-pytest libasan libubsan` | `python3-pytest` | `python-pytest` |
| Fuzzing | `clang compiler-rt llvm` | `clang libclang-rt-dev llvm` | `clang compiler-rt llvm` |
| VM regression | `virtme-ng qemu-system-x86-core acpica-tools python3-pytest python3-dbusmock python3-evdev python3-dbus dbus-daemon kernel-devel-$(uname -r)` | `virtme-ng qemu-system-x86 acpica-tools python3-pytest python3-dbusmock python3-evdev python3-dbus dbus-daemon linux-headers-$(uname -r)`, readable `/boot/vmlinuz-*` | `virtme-ng qemu-system-x86 acpica python-pytest python-dbusmock python-evdev python-dbus dbus`, the running kernel's `-headers` package |
| Hardware smoke | Python 3, root; `python3-matplotlib` for the plot | Python 3, root; `python3-matplotlib` for the plot | Python 3, root; `python-matplotlib` for the plot |

### Kernel and KVM

- The VM layer builds `test/module/thermald_test_kern_module.c` for a kernel
  and boots that kernel in the VM, by default the running one. Its
  headers must be installed: `/lib/modules/<release>/build` must exist.
- If the running kernel is self-built and has no headers, use an
  installed distribution kernel instead:
  `THD_VM_KERNEL=6.17.0-41-generic test/run_all.sh`. It needs the
  kernel image, readable (`/boot/vmlinuz-<release>`, or
  `/usr/lib/modules/<release>/vmlinuz` on Arch and CachyOS),
  `/lib/modules/<release>` and the headers (`linux-headers-<release>`,
  `kernel-devel-<release>`, or the kernel package's `-headers`).
- If your distribution's repositories have moved on to a newer kernel,
  reboot into it, use `THD_VM_KERNEL` with an older installed kernel
  that has headers, or get the matching headers package from the build
  system (on Fedora,
  `https://kojipkgs.fedoraproject.org/packages/kernel/<version>/<release>/x86_64/`).
- On Arch and CachyOS a kernel upgrade removes the running kernel's
  `/usr/lib/modules/<release>`; reboot into the new kernel before
  running the VM layer.
- `/dev/kvm` must be readable and writable by you. Desktop sessions
  usually get an ACL on it; otherwise `sudo usermod -aG kvm $USER` and
  log in again.
- The VM kernel needs `CONFIG_POWERCAP`, `CONFIG_INT340X_THERMAL`
  (`int3400_thermal`), `CONFIG_INPUT_UINPUT` and `CONFIG_ACPI_DEBUG`.
  Fedora, Ubuntu, Arch and CachyOS kernels have them.
  `CONFIG_THERMAL_EMULATION` is not needed.
- The XML fuzzer runs in a user namespace (`unshare -r`), because the
  parsers only read root-owned files. Ubuntu 24.04 and later restrict
  them: `sudo sysctl kernel.apparmor_restrict_unprivileged_userns=0`,
  or run `test/fuzz/run.sh xml` as root.
- No root is needed for the unit, fuzz and VM layers. Inside the VM the
  tests run as root, but `/etc` is a throw-away overlay and nothing on
  the host changes.

## Configuring the tree

All layers use the thermald built in this tree, configured with
`prefix=/` (the VM tests rely on `/etc/thermald` and `/var/run/thermald`):

    ./autogen.sh prefix=/
    make -j$(nproc)

`run_all.sh` and `regression/run.sh` run `make` again themselves.

## Running

Everything except the hardware test:

    test/run_all.sh
    test/run_all.sh --vault-dir /private/vaults     also real data vaults
    test/run_all.sh --fuzz-seconds 600 --skip-vm
    test/run_all.sh --report /tmp/thermald-report   also write a test report

It prints `unit:`, `fuzz:` and `vm:` with PASS or FAIL and exits non-zero
if any failed. A full run takes about 7 minutes, most of it the VM
scenarios.

One layer at a time:

| Layer | Command | Time |
|---|---|---|
| Parser unit tests | `make -C test/unit && pytest test/unit` | ~40 s |
| Fuzzing | `test/fuzz/run.sh` (`FUZZ_SECONDS=3600` for longer, `test/fuzz/run.sh xml` for one target) | 60 s per target |
| D-Bus fuzzing | `test/regression/run.sh -k dbus_fuzz` (`THD_DBUS_FUZZ_SEED=N THD_DBUS_FUZZ_CALLS=N` for others) | ~20 s |
| VM regression | `test/regression/run.sh` | ~6 min |
| One VM scenario | `test/regression/run.sh -k multizone` | ~10-80 s |
| Hardware smoke | `sudo test/hw/hw_smoke.py --seconds 120 --load 60` | as given |

Other options:

- `THERMALD_BIN=/path/to/thermald test/regression/run.sh` tests another
  build, e.g. to check that a new scenario fails without a fix.
- `THD_TEST_DEBUG=1 test/unit/build/gddv_dump vault.bin` shows the
  parser's debug messages.
- A fuzzer crash input can be replayed with
  `test/unit/build-fuzz/fuzz_<target> <crash file>`; for `xml` inside
  `unshare -r`, with `THD_FUZZ_LOG=1` to see thermald's messages.
- `make -C test/unit thermald` builds thermald with ASan and UBSan
  (`test/unit/build/thermald`); `THERMALD_BIN=$PWD/test/unit/build/thermald
  test/regression/run.sh` runs every scenario with it.

The hardware smoke test runs on a real, supported machine: stop the
thermald service first (`systemctl stop thermald`; the script refuses
to run otherwise). It changes the real power limits while it runs and
checks that they are all restored when thermald exits. Exit status 0
pass, 1 fail, 2 could not run (e.g. thermald refuses the platform).

## Results

| Layer | Where |
|---|---|
| Unit tests | pytest output |
| Fuzzing | `/tmp/thermald-fuzz/<target>/` (`$THD_FUZZ_OUT`): `corpus/`, `crashes/` |
| VM regression | `/tmp/thermald-regression/<scenario>/` (`$THD_REG_OUT`): `result.json` (status, trace of temperatures and PL1, every RAPL write), `thermald.log`, `dmesg.log`, `vm.log`, the generated `data_vault.bin` and `ietm.asl` |
| Hardware smoke | `/tmp/thermald-hw/<host>-<time>/`: `report.json`, `thermald.log`, `plot.png` (temperatures, package power and PL1 over time), `trace.csv` (the same data) |

## Test report

To produce a report that can be attached to a submission (patch, pull
request, release):

    test/run_all.sh --report /tmp/thermald-report \
        [--vault-dir /private/vaults] \
        [--hw-report /tmp/thermald-hw/<host>-<time>/report.json]

This writes `/tmp/thermald-report/report.md`: the machine, OS, kernel,
CPU and the thermald branch and commit (marked if there are uncommitted
changes), a summary per layer, every unit test and VM scenario with its
result and time (and the failure message for any that failed), the fuzz
run's numbers, and for each `--hw-report` the hardware smoke test result
with its plot (copied next to the report). Run the hardware smoke test
first, as root, to have a `report.json` to include. Private data vaults
are only counted (`real vault 1`, ...), never named or included.

The JUnit XML files (`unit.xml`, `vm.xml`) and `fuzz.log` are kept in
the same directory. `test/report.py DIR [--hw ...]` writes the report
again from them.

## Real data vaults

Real vaults may contain OEM data, so they never go into this repo. Keep
them in a private directory, one `<name>.bin` per platform (a copy of
`/sys/bus/platform/devices/INTC*/data_vault`, readable by root), and
pass the directory with `--vault-dir` (or `THD_VAULT_DIR`). Each vault
is:

- parsed by the unit tests, which must accept it,
- used as a fuzzing seed,
- run in the VM on a platform built from its own tables (sensors,
  cooling devices, PPCC), which must stay in adaptive mode, keep PL1
  within the PPCC range and restore it on exit.

An optional `<name>.json` next to a vault adds what the vault doesn't
contain (OEM variables, CPU model, power source, switches) and expected
results; see `regression/lib/real_vault.py` and `test_real_vault` in
`unit/test_gddv_parser.py`. The fuzzer copies the vaults into its own
directory under `/tmp`.

## What the tests check

### Parser unit tests (`unit/test_gddv_parser.py`)

Each test builds a vault, runs thermald's parser on it (ASan + UBSan)
and checks the result. Any sanitizer error is a failure.

| Test | Checks |
|---|---|
| `test_psvt_entries` | PSVT tables: several tables and entries, MIN/MAX/numeric limits, temperatures, sample period, priority |
| `test_ppcc_two_limits` | PPCC with PL1 and PL2: every limit, time window and step |
| `test_ppcc_pl1_only` | PPCC with only PL1 is kept, PL2 marked invalid |
| `test_apct_apat` | condition sets with several ANDed conditions, devices, comparisons; targets with their codes and arguments |
| `test_two_virtual_sensors_stay_separate` | VSCT/VSPT of two virtual sensors aren't merged (6656673) |
| `test_itmt_v1_short_last_entry` | ITMT v1 whose last entry ends after pl1_max, rest of vault still used (d0929bb) |
| `test_itmt_v1_two_full_entries` | ITMT v1 with complete entries |
| `test_every_truncation` | every prefix of a valid vault with all table types |
| `test_random_mutations` | 400 vaults with random byte changes (fixed seeds) |
| `test_huge_lengths` | key and string lengths up to 0xffffffff are rejected |
| `test_length_near_int_max` | key, value and string lengths just below INT_MAX, where `offset + length` overflowed (found by fuzzing) |
| `test_compressed_vault` | an LZMA-compressed v2 vault, as firmware ships, parses |
| `test_compressed_vault_with_short_header` | a compressed vault whose header is shorter than the v2 header (found by fuzzing) |
| `test_real_vault` | each private vault parses, and matches the `"parser"` expectations in its `.json` |

### Fuzzing (`fuzz/`)

libFuzzer mutates inputs and runs a parser on each; it stops at the
first sanitizer error, crash or failed check and saves the input. All
targets are built with ASan and UBSan.

| Target | What it runs | Seeds |
|---|---|---|
| `gddv` | the data vault parser (`parse_gddv()`, `merge_appc()`): header, nested and LZMA-compressed segments, key/value readers, every table type | one vault per table type, plus private vaults |
| `xml` | each input as `thermal-conf.xml`, `thermald-features.xml` and `thermal-cpu-cdev-order.xml`, read by the real `parser_init()` (file checks, DTD rejection, libxml2 options), then parsed, dumped, matched to the platform and read back through the getters thermald uses | `data/*.xml`, `manual/configs/`, `regression/configs/`, one matching any platform; a dictionary of the element names the parsers look for |
| `util` | `parse_int_value()`, `parse_double_value()` (result within the range and finite, or rejected) and `is_valid_thermal_object_name()` (never accepts `/`) | a few values |

The `xml` target is built with `TDCONFDIR` set to
`$THD_FUZZ_OUT/xml/conf`, where it writes each input.

D-Bus input can't be fuzzed in-process (the handlers need a running
engine), so the VM scenarios `dbus_fuzz_adaptive` and `dbus_fuzz_default`
do it against the daemon, see below.

### VM regression scenarios (`regression/scenarios/`)

Each scenario boots a VM with its own fake platform and runs the
thermald binary from this tree.

| Area | Scenario | Checks |
|---|---|---|
| Power limits | `psvt_throttle_restore` | PL1 starts at the PPCC maximum, a PSVT trip throttles it toward the PPCC minimum (never below), it returns to the maximum when the zone cools, and the power-on PL1 is restored on SIGTERM |
| | `ppcc_pl1_only` | a PPCC with only PL1 is used for RAPL |
| | `rapl_msr_parking` | with MMIO RAPL, MMIO PL1 controls, the MSR PL1 is parked at 200 W, the package draws the MMIO limit, both are restored on exit |
| | `psvt_temperature_out_of_range` | a PSVT entry with an absurd temperature is ignored with a warning instead of becoming a wrapped trip point; the other entries still work |
| | `rapl_bios_locked` | when power limit writes fail (BIOS lock) thermald keeps running, leaves PL1 alone, stops retrying and exits cleanly |
| | `psvt_missing_fallback` | a target naming a PSVT missing from the GDDV installs the default PSVT (IETM.D0), not another table, and adaptive mode keeps running (Dell Precision 3660) |
| | `psvt_missing_no_default` | the same without a default PSVT: thermald says so and gives up adaptive mode as before |
| ITMT | `itmt_throttle_restore` | an ITMT target (APAT `ITMT` action on IETM) installs its table: TCPU above 60 C caps PL1 at 25 W, above 70 C at 15 W, back to 25 W and then the 35 W PPCC maximum as it cools, power-on PL1 restored on exit |
| | `itmt_min_limit_b0d4` | an ITMT entry naming B0D4 uses the TCPU zone; a `MIN` limit goes to the PPCC minimum |
| | `itmt_psvt_switch` | switching between an ITMT and a PSVT target (power slider) replaces the trips: only the selected target's sensor limits PL1 |
| | `itmt_requires_idsp` | without the ITMT UUID in the IDSP the table is skipped and nothing throttles; with no other policy thermald writes ignore_adaptive and exits |
| Condition tables | `conditions_power_lid_tablet` | target selection from AC/battery (UPower), lid and tablet-mode switches, in table order, through 6 switches |
| | `conditions_oem_slider_temp` | target selection from OEM variables (ODVP), the power slider (power-profiles-daemon), a temperature, and two ANDed conditions |
| | `conditions_motion` | Motion conditions are supported (evaluated as stationary) instead of forcing the fallback target |
| | `conditions_unsupported_fallback` | when no condition set can be met, the highest-power target is applied |
| Temperature limits, restore | `reinit_while_throttled` | after a D-Bus Reinit, thermald (still adaptive) controls PL1 and restores the original power-on PL1 on exit |
| | `xml_pl1_staircase` | non-adaptive `thermal-conf.xml` (`configs/pl1_staircase.xml`): several passive trips in one zone, PL1 steps down and back up one trip at a time instead of jumping to the maximum, PL2 trip with a large hysteresis |
| Several zones | `multizone_shared_cdev` | two zones on one cooling device: the stricter limit wins, one zone cooling doesn't release the other's limit, release when both are cool |
| Data vault | `vault_itmt_v1_short` | the short ITMT v1 entry doesn't stop the adaptive engine |
| | `vault_malformed_tables` | truncated APCT and oversized APAT string: no crash, clean exit |
| | `vault_bad_count` | APCT claiming 0xffffffff conditions is rejected |
| | `vault_corrupt_header` | garbage vault: adaptive mode fails, thermald keeps running |
| | `vault_corrupt_adaptive_only_cpu` | same on an adaptive-only CPU: clean exit with status 2 |
| Recent fixes | `virtual_sensor_precision` | a virtual sensor keeps millidegree precision: 30.5 C doesn't match a 30.8 C condition, 31.0 C does |
| | `ietm_unsupported_action` | an IETM action thermald doesn't handle (EPOT) doesn't change the trips the target's PSVT installed (39 C + thermald's 40 C, not 40 + 40) |
| | `virtual_sensors_two_participants` | two virtual sensors each get a zone; only the one in the PSVT throttles (6656673) |
| | `input_switch_removed` | removing the lid switch device doesn't make thermald spin, the lid then counts as open (PR #611) |
| | `notify_platform_imok` | NotifyPlatform targets call IETM.IMOK with their argument, only for IETM (PR #613) |
| | `ignore_adaptive_flow` | when no adaptive zone can be created thermald writes ignore_adaptive and exits; the next start runs non-adaptive |
| | `late_devices_retry` | the zone and RAPL appear only after thermald started, as when drivers probe late at boot: thermald exits without writing ignore_adaptive and the next start runs adaptive, once per boot |
| D-Bus | `dbus_fuzz_adaptive`, `dbus_fuzz_default` | 3000 random calls (fixed seed) to every D-Bus method except Terminate, with the method list and argument types read from `src/thd_dbus_interface.xml`: names of real and missing zones, sensors and cooling devices, path traversal, printf conversions, long and non-ASCII strings, 0, -1, INT_MIN/INT_MAX, UINT32_MAX, NaN, infinity. thermald (the ASan + UBSan build) must answer every call, keep running, report no sanitizer error or leak and exit cleanly. In the adaptive engine and in the default engine with an XML config. Calls in `dbus_calls.log` |
| Real vaults | `test_real_vault` (`real/real_vault.py`) | each private vault: stays adaptive, every PSVT target has a zone, PL1 within the PPCC range, power-on PL1 restored on exit, plus the `"vm"` expectations in its `.json` |

### Hardware smoke test (`hw/hw_smoke.py`)

On the real machine: thermald starts and keeps running in adaptive mode
with no missing zones, every PL1 it writes is within the PPCC range it
logged, and every powercap limit, time window and enable bit is back to
its original value after exit. It reports unsupported conditions, a
fallback target and a leftover `ignore_adaptive` as notes.

Plot: `plot.png` has two charts on one time axis. On top, the sensors
thermald uses to limit power (zones in its last zone dump with a passive
trip on a RAPL cooling device), each with its trip temperatures as thin
lines, then the CPU package temperature and the hottest other sensor, up
to three lines. Below, measured package power and the PL1 limit. The
load phase is shaded. So you can see a sensor cross its trip and PL1
respond. `trace.csv`
has the same samples. To redraw it from a report:
`test/hw/plot_trace.py /tmp/thermald-hw/<host>-<time>/report.json`.

Options:

- thermald runs with `--ignore-cpuid-check` by default, so that
  platforms it would otherwise refuse can be tested: CPU models not in
  its list yet (pre-production) and ThinkPads exposing
  `thinkpad_acpi/dytc_lapmode`, whose firmware normally owns thermal
  policy. `--cpuid-check` keeps thermald's normal check, to test that
  check itself; SKIP then means thermald refused the platform.
- thermald's log is `thermald.log` next to `report.json`, at
  `--loglevel=info` (enough for the checks); `--loglevel debug` for
  more.
- `--seconds N` (default 60), `--load N` (N seconds of full CPU load in
  the middle of the run), `--no-adaptive`, `--thermald PATH`.

## Manual tests

These predate the automated suites and run by hand on a real machine as
root. They change the installed configuration and real power limits.
Most of what they check is now covered by the VM regression suite.

| Directory | What | How |
|---|---|---|
| `manual/emulation/` | XML configs (`test1.xml` .. `test7.xml`) and RAPL, intel_pstate, powerclamp, processor and default cooling, with temperatures set through the test module and `emul_temp` (needs `CONFIG_THERMAL_EMULATION=y`) | `manual/emulation/README.txt`; `sudo ./exec_config_tests.sh` or `./run_all_tests.sh` from that directory |
| `manual/configs/` | a walkthrough of `thermal_conf.xml.1` .. `.7` (single step, PID, multiple cooling devices, ...) with the expected behaviour | `manual/configs/test_cases.txt` |
| `manual/data_vault/` | reproducing another machine's adaptive tables: a test zone plus a data vault in `/lib/firmware/intel/dtt/` | `manual/data_vault/README.txt` |

## Debug tools

Not tests, but used with them when something fails on a real machine.
They are in `tools/`:

| Script | Use |
|---|---|
| `thermal-debug-dump-fedora.sh`, `thermal-debug-dump-ubuntu.sh` | build current thermald, run it with a debug log under `stress-ng` load and pack the log, power limits and ACPI tables for bug reports |
| `thermal-dptf-tables-test-fedora.sh`, `-ubuntu.sh` | build current thermald and run adaptive mode with `--test-mode`, to see whether a machine's DTT tables can be used |
| `thermald_debug_output_graph.py` | plot temperatures and trips from a debug log |
| `thermal_temp_dump.py`, `thermal_config_dump.py` | dump temperatures (every 10 s) and the active configuration from a running thermald over D-Bus |
| `thermald_set_pref.sh` | a D-Bus client: preference, reinit, terminate, add sensors and zones |

## Writing tests

- VM scenarios: see `regression/README.md`. Configs a scenario loads go
  in `regression/configs/`.
- Parser tests: add a function to `unit/test_gddv_parser.py`; build the
  vault with `regression/lib/gddv.py` and check the parsed JSON.
- When a test covers a bug fix, check that it fails without the fix
  (build the unfixed tree in a `git worktree` and use `THERMALD_BIN`).

## Known bugs

Tests for thermald bugs that aren't fixed yet are strict expected
failures (pytest `xfail(strict=True)`, or `known_bug` in a VM
scenario). They show as XFAIL, and fail once the bug is fixed so the
marker is removed together with the fix. The current list is in each
test's reason text; `pytest -rx` lists them.

UBSan's alignment check is off: the parser reads integers at unaligned
offsets (`*(uint64_t *)` on the vault buffer). The data vault exists only
on x86 (Intel IETM/INT3400), where unaligned loads are fine.

## Troubleshooting

| Symptom | Cause |
|---|---|
| `Missing: kernel-headers-for-...` | install the headers for that kernel, or set `THD_VM_KERNEL` |
| `Missing: readable-/boot/vmlinuz-...` | `sudo chmod 0644 /boot/vmlinuz-<release>` (Ubuntu) |
| `cannot find /usr/lib64/libasan.so...` | install `libasan libubsan` |
| `Could not access KVM kernel module` | `/dev/kvm` permissions |
| `pahole version differs` warning | harmless, from building the module |
| A VM scenario fails with "no result from VM" | see `vm.log` in its result directory |
