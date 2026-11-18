#!/bin/bash
# Build thermald and the test module, then run the regression scenarios.
#
#   test/regression/run.sh              all scenarios
#   test/regression/run.sh -k restore   scenarios matching "restore"
#   test/regression/run.sh --vault-dir /private/vaults   add real vaults
#
# Extra arguments go to pytest. Each scenario boots its own VM with
# virtme-ng, using the running kernel. Results: $THD_REG_OUT/<scenario>/
# (default /tmp/thermald-regression). Set THERMALD_BIN to test a
# thermald binary other than the one in the repo root, THD_VM_KERNEL to
# boot another installed kernel in the VM.
#
# Needs: virtme-ng, qemu with KVM, iasl (acpica-tools), pytest and
# kernel-devel for the running kernel.

set -e

# --vault-dir DIR: also run every DIR/*.bin data vault dump (with an
# optional DIR/<name>.json, see lib/real_vault.py). Never copy them here.
args=()
while [ $# -gt 0 ]; do
	case "$1" in
	--vault-dir) export THD_VAULT_DIR=$(cd "$2" && pwd); shift 2 ;;
	--vault-dir=*) export THD_VAULT_DIR=$(cd "${1#*=}" && pwd); shift ;;
	*) args+=("$1"); shift ;;
	esac
done
set -- "${args[@]}"

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)

missing=
for tool in vng qemu-system-x86_64 iasl pytest; do
	command -v $tool >/dev/null || missing="$missing $tool"
done
# THD_VM_KERNEL=<release>: boot that installed kernel in the VM instead
# of the running one (e.g. when the running kernel is self-built and has
# no headers). It needs the kernel image, /boot/vmlinuz-<release> or
# /lib/modules/<release>/vmlinuz (Arch, CachyOS), readable,
# /lib/modules/<release> and its headers.
KVER=${THD_VM_KERNEL:-$(uname -r)}
if [ ! -e "/lib/modules/$KVER/build/Makefile" ]; then
	missing="$missing kernel-headers-for-$KVER"
fi
if [ -n "$THD_VM_KERNEL" ] && [ ! -r "/boot/vmlinuz-$KVER" ] &&
		[ ! -r "/lib/modules/$KVER/vmlinuz" ]; then
	missing="$missing readable-/boot/vmlinuz-$KVER"
fi
if [ -n "$missing" ]; then
	echo "Missing:$missing" >&2
	exit 2
fi

if [ -z "$THERMALD_BIN" ]; then
	if [ ! -e "$REPO/Makefile" ]; then
		echo "Configure the tree first (./autogen.sh && ./configure prefix=/)" >&2
		exit 2
	fi
	make -C "$REPO" -j"$(nproc)" -s
fi
make -C "$REPO/test/module" -s KVER="$KVER" >/dev/null
# Parser tool, and thermald with ASan for "sanitized" scenarios (dbus_fuzz)
make -C "$REPO/test/unit" -j"$(nproc)" -s all thermald

cd "$HERE"
exec pytest -v "$@"
