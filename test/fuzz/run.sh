#!/bin/bash
# Fuzz thermald's input parsers with libFuzzer (clang, ASan, UBSan).
#
#   test/fuzz/run.sh                 every target, 60 s each
#   test/fuzz/run.sh xml             only the given targets
#   FUZZ_SECONDS=3600 test/fuzz/run.sh
#   THD_VAULT_DIR=/private/vaults test/fuzz/run.sh   add real vaults as seeds
#
# Targets:
#   gddv  the data vault (GDDV) parser
#   xml   thermal-conf.xml, thermald-features.xml and
#         thermal-cpu-cdev-order.xml parsers (runs in a user namespace,
#         see fuzz_xml.cpp)
#   util  integer, floating point and object name helpers
#
# D-Bus methods are fuzzed against a running thermald by the VM
# scenario dbus_fuzz (test/regression).
#
# The corpus and any crash inputs stay outside the repo, in
# $THD_FUZZ_OUT/<target> (default /tmp/thermald-fuzz); real vaults may
# contain OEM data. Exit status is non-zero if a target found a crash.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$HERE/../.." && pwd)
OUT=${THD_FUZZ_OUT:-/tmp/thermald-fuzz}
SECONDS_TO_RUN=${FUZZ_SECONDS:-60}
TARGETS=${*:-gddv xml util}

command -v clang++ >/dev/null || { echo "Missing: clang (and compiler-rt)" >&2; exit 2; }

# FUZZ_RECOVER_OVERFLOW=1: report signed integer overflow but keep
# going, to look for other bugs while a known overflow is unfixed.
RECOVER=
if [ -n "$FUZZ_RECOVER_OVERFLOW" ]; then
	RECOVER="-fsanitize-recover=signed-integer-overflow"
	BUILD=build-fuzz-recover
else
	BUILD=build-fuzz
fi

# The XML parsers read their files from TDCONFDIR
CONFDIR=$OUT/xml/conf
mkdir -p "$CONFDIR"

make -C "$HERE/../unit" -j"$(nproc)" -s CC=clang CXX=clang++ BUILD=$BUILD \
	CONFDIR="$CONFDIR" \
	SANITIZE="-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all $RECOVER" \
	fuzz

# XML element names the parsers look for, so the fuzzer finds them
xml_dict() {
	grep -oh 'thd_strcasecmp_n([^"]*"[A-Za-z0-9_]*"' \
		"$TOP"/src/thd_parse.cpp "$TOP"/src/thd_features_parse.cpp \
		"$TOP"/src/thd_cdev_order_parser.cpp | grep -o '"[A-Za-z0-9_]*"' |
		sort -u | sed 's/"\(.*\)"/"<\1>"\n"<\/\1>"/'
}

seeds() {
	local target=$1 dir=$2
	case $target in
	gddv)
		python3 "$HERE/make_seeds.py" "$dir"
		if [ -n "$THD_VAULT_DIR" ]; then
			cp "$THD_VAULT_DIR"/*.bin "$dir/" 2>/dev/null || true
		fi
		;;
	xml)
		cp "$TOP"/data/*.xml "$dir/"
		for f in "$TOP"/test/manual/configs/thermal_conf.xml.* \
			 "$TOP"/test/regression/configs/*.xml; do
			cp "$f" "$dir/$(basename "$f").xml"
		done
		# Values that are only white space
		printf '<?xml version="1.0"?>\n<ThermaldFeatures><DbusControl> \t </DbusControl></ThermaldFeatures>\n' \
			> "$dir/blank_value.xml"
		# Matches any machine, so the getters run on the parsed data
		sed -e 's|<ProductName>[^<]*</ProductName>|<ProductName>*</ProductName>|' \
			"$TOP"/test/regression/configs/pl1_staircase.xml > "$dir/any_platform.xml"
		;;
	util)
		printf '%s' 42 > "$dir/int"
		printf '%s' "-1.5e3 " > "$dir/double"
		printf '%s' "x86_pkg_temp" > "$dir/name"
		printf '%s' "../../sys" > "$dir/path"
		;;
	esac
}

# Run as given, or in a user namespace where created files are owned by
# uid 0 (the XML parsers only read root-owned files)
runner() {
	if [ "$1" != xml ] || [ "$(id -u)" = 0 ]; then
		return 0
	fi
	if ! unshare -r true 2>/dev/null; then
		echo "fuzz xml: needs root or unprivileged user namespaces" \
			"(Ubuntu: sysctl kernel.apparmor_restrict_unprivileged_userns=0)" >&2
		return 1
	fi
	echo "unshare -r"
}

status=0
for target in $TARGETS; do
	bin=$HERE/../unit/$BUILD/fuzz_$target
	[ -x "$bin" ] || { echo "Unknown fuzz target: $target" >&2; exit 2; }
	dir=$OUT/$target
	mkdir -p "$dir/corpus" "$dir/seeds" "$dir/crashes"
	seeds "$target" "$dir/seeds"
	opts=(-max_total_time="$SECONDS_TO_RUN" -timeout=10
	      -artifact_prefix="$dir/crashes/" -print_final_stats=1)
	case $target in
	gddv) opts+=(-max_len=65536) ;;
	xml)  xml_dict > "$dir/xml.dict"; opts+=(-max_len=16384 -dict="$dir/xml.dict") ;;
	util) opts+=(-max_len=256) ;;
	esac

	echo "=== fuzz target: $target ==="
	if ! prefix=$(runner "$target"); then
		echo "=== fuzz target $target: not run ==="
		status=1
		continue
	fi
	# shellcheck disable=SC2086
	if $prefix "$bin" "${opts[@]}" "$dir/corpus" "$dir/seeds"; then
		echo "=== fuzz target $target: no findings ==="
	else
		echo "=== fuzz target $target: FOUND A CRASH (inputs in $dir/crashes) ==="
		status=1
	fi
done
exit $status
