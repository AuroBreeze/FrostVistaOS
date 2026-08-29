ARCH_MK_PATH := arch/loongarch/mk

.PHONY: qemu

# Keep the common qemu typo from failing before the run target is reached.

include $(ARCH_MK_PATH)/arch-loongarch.mk
include $(ARCH_MK_PATH)/source.mk
include $(ARCH_MK_PATH)/build.mk
include $(ARCH_MK_PATH)/images.mk

include $(ARCH_MK_PATH)/run.mk
