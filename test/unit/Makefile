# Unit test and fuzzing helpers built from thermald's own sources.
#
#   make -C test/unit          build/gddv_dump (ASan + UBSan)
#   test/fuzz/run.sh           libFuzzer targets, built with clang
#
# The source list is read from the top-level Makefile.am, so it follows
# the real build. The tree must be configured (config.h and the
# generated thermald-resource.c come from ./configure and make).

TOP := $(abspath ../..)
BUILD := build

SRCS := $(filter-out src/main.cpp,$(shell sed -n '/^thermald_SOURCES = /,/^$$/p' \
	$(TOP)/Makefile.am | tr -d '\\' | tr -s ' \t' '\n' | \
	grep -v -e '^$$' -e '=' -e 'thermald_SOURCES'))

PKGS := glib-2.0 gio-2.0 gmodule-2.0 libxml-2.0 upower-glib libevdev
# The GDDV parser reads integers at unaligned offsets (*(uint64_t *) on
# a byte buffer). The data vault only exists on x86, where that is fine,
# so alignment checks are off; all other UBSan and ASan errors are fatal.
SANITIZE ?= -fsanitize=address,undefined -fno-sanitize=alignment \
	-fno-sanitize-recover=all -fno-omit-frame-pointer
# -MMD -MP: rebuild objects when a header they include changes
# The XML fuzzer writes its inputs where the parsers look for them
CONFDIR ?= /etc/thermald
CPPFLAGS := -MMD -MP -I$(TOP) -I$(TOP)/src $(shell pkg-config --cflags $(PKGS)) \
	-DTDRUNDIR=\"/var/run/thermald\" -DTDCONFDIR=\"$(CONFDIR)\" \
	-DTDLOCALEDIR=\"/usr/share/locale\" -DGLIB_SUPPORT
CXXFLAGS ?= -O1 -g
CFLAGS ?= -O1 -g
LDLIBS := $(shell pkg-config --libs $(PKGS)) -lm -ldl

OBJS := $(patsubst %,$(BUILD)/%.o,$(SRCS))

all: $(BUILD)/gddv_dump

# Rebuild everything when the flags change (CONFDIR, SANITIZE, ...)
FLAGS := $(CPPFLAGS) $(CXXFLAGS) $(CFLAGS) $(SANITIZE) $(CC) $(CXX)
$(BUILD)/.flags: FORCE
	@mkdir -p $(BUILD)
	@echo '$(FLAGS)' | cmp -s - $@ || echo '$(FLAGS)' > $@

$(BUILD)/%.cpp.o: $(TOP)/%.cpp $(BUILD)/.flags
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SANITIZE) -c $< -o $@

$(BUILD)/%.c.o: $(TOP)/%.c $(BUILD)/.flags
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZE) -c $< -o $@

$(BUILD)/main_stubs.o: main_stubs.cpp $(BUILD)/.flags
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SANITIZE) -c $< -o $@

$(BUILD)/gddv_dump.o: gddv_dump.cpp gddv_test.h $(BUILD)/.flags
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SANITIZE) -c $< -o $@

$(BUILD)/gddv_dump: $(BUILD)/gddv_dump.o $(BUILD)/main_stubs.o $(OBJS)
	$(CXX) $(SANITIZE) $^ $(LDLIBS) -o $@

# thermald itself with ASan and UBSan, for the VM scenarios:
#   make -C test/unit thermald
#   THERMALD_BIN=test/unit/build/thermald test/regression/run.sh dbus_fuzz
thermald: $(BUILD)/thermald

$(BUILD)/thermald: $(BUILD)/src/main.cpp.o $(OBJS)
	$(CXX) $(SANITIZE) $^ $(LDLIBS) -o $@

# libFuzzer targets (test/fuzz/run.sh builds them):
#   make CC=clang CXX=clang++ BUILD=build-fuzz CONFDIR=... \
#	SANITIZE="-fsanitize=fuzzer-no-link,address,undefined ..." fuzz
FUZZERS := fuzz_gddv fuzz_xml fuzz_util
fuzz: $(addprefix $(BUILD)/,$(FUZZERS))

$(BUILD)/fuzz_%.o: ../fuzz/fuzz_%.cpp $(BUILD)/.flags
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) -I. $(CXXFLAGS) $(SANITIZE) -c $< -o $@

$(BUILD)/fuzz_%: $(BUILD)/fuzz_%.o $(BUILD)/main_stubs.o $(OBJS)
	$(CXX) $(SANITIZE) -fsanitize=fuzzer $^ $(LDLIBS) -o $@

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)

clean:
	rm -rf $(BUILD) build-fuzz build-fuzz-recover

.PHONY: all clean fuzz thermald FORCE
