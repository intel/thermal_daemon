obj-m += thermald_test_kern_module.o

# Kernel to build for: the running one, or KVER=<release> (e.g. the
# kernel the regression VM boots, see THD_VM_KERNEL in test/README.md)
KVER ?= $(shell uname -r)
KDIR := /lib/modules/$(KVER)/build

# A kernel built with clang (e.g. CachyOS) needs its modules built with
# the LLVM toolchain too (clang, ld.lld, llvm-ar, ...)
ifeq ($(shell grep -s '^CONFIG_CC_IS_CLANG=y' $(KDIR)/.config),CONFIG_CC_IS_CLANG=y)
LLVM ?= 1
endif

all:
	make -C $(KDIR) M=$(CURDIR) $(if $(LLVM),LLVM=$(LLVM)) modules

clean:
	make -C $(KDIR) M=$(CURDIR) $(if $(LLVM),LLVM=$(LLVM)) clean
