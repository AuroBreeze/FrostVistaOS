MAKEFLAGS += -j$(shell nproc)

include mk/config.mk
include mk/toolchain.mk

ifeq ($(ARCH), riscv)
	include mk/arch-riscv.mk
else
	$(error Unsupported ARCH=$(ARCH). Use ARCH=riscv)
endif

# no dependencies
include mk/fs.mk

# dependencies
include mk/sources.mk


.PHONY: all clean clean_disk run run-sbi run-sbi-ext4 run-contest-rv \
        build_test disasm lint format qemu compdb tidy tidy-file debug gdb
# ---
include mk/build.mk
include mk/images.mk
include mk/run.mk

include mk/checks.mk
include mk/clean.mk
