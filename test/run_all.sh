#!/bin/bash
# Run all thermald tests: parser unit tests, a short fuzz run of each
# fuzz target and the VM regression scenarios (which include random D-Bus
# calls to an ASan build of thermald).
#
#   test/run_all.sh
#   test/run_all.sh --vault-dir /private/vaults   also real data vaults
#   test/run_all.sh --fuzz-seconds 600 --skip-vm   600 s per fuzz target
#   test/run_all.sh --report DIR [--hw-report .../report.json]
#
# --report DIR keeps each layer's results in DIR and writes DIR/report.md
# (test/report.py), a report that can be attached to a submission;
# --hw-report adds a hardware smoke test run (repeatable).
#
# Real vaults are read from the given directory only (they may hold OEM
# data); nothing is copied into the repo. The hardware smoke test
# (test/hw/hw_smoke.py) is separate: it needs root and a supported
# machine, and changes the real power limits while it runs.

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
FUZZ_SECONDS=30
SKIP_VM=
VAULT_DIR=
REPORT=
HW_REPORTS=()

while [ $# -gt 0 ]; do
	case "$1" in
	--vault-dir) VAULT_DIR=$(cd "$2" && pwd); shift 2 ;;
	--fuzz-seconds) FUZZ_SECONDS=$2; shift 2 ;;
	--skip-vm) SKIP_VM=1; shift ;;
	--report) mkdir -p "$2" && REPORT=$(cd "$2" && pwd); shift 2 ;;
	--hw-report) HW_REPORTS+=(--hw "$(readlink -f "$2")"); shift 2 ;;
	*) echo "unknown option $1" >&2; exit 2 ;;
	esac
done

if [ ! -e "$REPO/Makefile" ]; then
	echo "Configure the tree first (./autogen.sh prefix=/)" >&2
	exit 2
fi

declare -A status
run() {
	local name=$1; shift
	echo "=== $name"
	if "$@"; then status[$name]=PASS; else status[$name]=FAIL; fi
}

make -C "$REPO" -j"$(nproc)" -s || exit 1

UNIT_XML= VM_XML=() FUZZ_LOG=/dev/null
if [ -n "$REPORT" ]; then
	rm -f "$REPORT"/{unit.xml,vm.xml,fuzz.log,status.txt,report.md}
	# The commit the tests run on, even if the report is written later
	{ git -C "$REPO" rev-parse --abbrev-ref HEAD
	  git -C "$REPO" log -1 --format='%h %s'
	  git -C "$REPO" status --porcelain --untracked-files=no | head -1; } \
		> "$REPORT/commit.txt"
	UNIT_XML="--junitxml=$REPORT/unit.xml"
	VM_XML=("--junitxml=$REPORT/vm.xml")
	FUZZ_LOG=$REPORT/fuzz.log
fi

run unit bash -c "make -C '$HERE/unit' -j$(nproc) -s && \
	THD_VAULT_DIR='$VAULT_DIR' python3 -m pytest -q -p no:cacheprovider $UNIT_XML '$HERE/unit'"
run fuzz bash -c "set -o pipefail; FUZZ_SECONDS='$FUZZ_SECONDS' THD_VAULT_DIR='$VAULT_DIR' \
	'$HERE/fuzz/run.sh' 2>&1 | tee '$FUZZ_LOG' | grep -E '^(===|Done|SUMMARY|==[0-9]+==ERROR)'"
if [ -z "$SKIP_VM" ]; then
	if [ -n "$VAULT_DIR" ]; then
		run vm "$HERE/regression/run.sh" --vault-dir "$VAULT_DIR" "${VM_XML[@]}"
	else
		run vm "$HERE/regression/run.sh" "${VM_XML[@]}"
	fi
fi

echo
rc=0
for name in unit fuzz vm; do
	[ -n "${status[$name]}" ] || continue
	echo "$name: ${status[$name]}"
	[ "${status[$name]}" = PASS ] || rc=1
done

if [ -n "$REPORT" ]; then
	for name in unit fuzz vm; do
		[ -n "${status[$name]}" ] && echo "$name: ${status[$name]}"
	done > "$REPORT/status.txt"
	ls "$VAULT_DIR"/*.bin 2>/dev/null | wc -l > "$REPORT/vault_count.txt"
	echo "${THD_VM_KERNEL:-$(uname -r)}" > "$REPORT/vm_kernel.txt"
	echo "report: $(python3 "$HERE/report.py" "$REPORT" "${HW_REPORTS[@]}")"
fi
exit $rc
