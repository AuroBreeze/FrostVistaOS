MAKEFLAGS += -j$(shell nproc)

include mk/config.mk
include mk/toolchain.mk

include mk/fs.mk
include mk/images.mk

ifneq ($(filter $(ARCH), riscv loongarch),)
	include arch/$(ARCH)/Makefile
else
  $(error Unsupported ARCH=$(ARCH). Use ARCH=riscv or loongarch)
endif

.PHONY: disasm gdb lint format compdb tidy tidy-file

include mk/build.mk
include mk/run.mk

include mk/checks.mk
include mk/clean.mk
