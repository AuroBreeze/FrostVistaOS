# Source discovery and object list generation.
#
# Consumes:
# 	ARCH, ARCH_EXCLUDE_C, ARCH_EXCLUDE_S, OBJ_DIR
# Produces:
# 	KERNEL_C, OBJS, FORMAT_SRC, ARCH_C, ARCH_S

KERNEL_C := $(wildcard kernel/core/*.c)
KERNEL_C += $(wildcard kernel/mm/*.c)
KERNEL_C += $(wildcard kernel/mm/*/*.c)
# PCI enumeration/resources, transport, and common block request handling.
KERNEL_C += $(wildcard kernel/driver/pci*.c kernel/driver/virtio_pci*.c)
KERNEL_C += kernel/driver/virtio_blk.c

# Build the common filesystem layer and supported filesystem backends.
KERNEL_C += $(wildcard kernel/fs/*.c)
ifneq ($(filter easyfs,$(FS_LIST)),)
KERNEL_C += $(wildcard kernel/fs/easyfs/*.c)
endif
ifneq ($(filter ext4,$(FS_LIST)),)
KERNEL_C += $(wildcard kernel/fs/ext4fs/*.c)
endif
ifneq ($(filter tmpfs,$(FS_LIST)),)
KERNEL_C += $(wildcard kernel/fs/tmpfs/*.c)
endif
ifneq ($(filter devtmpfs,$(FS_LIST)),)
KERNEL_C += $(wildcard kernel/fs/devtmpfs/*.c)
endif

ARCH_C := $(wildcard arch/$(ARCH)/*/*.c)
ARCH_S := $(wildcard arch/$(ARCH)/*/*.S)

ARCH_C := $(filter-out arch/$(ARCH)/bootloader/%, $(ARCH_C))
ARCH_S := $(filter-out arch/$(ARCH)/bootloader/%, $(ARCH_S))

ARCH_C := $(filter-out $(ARCH_EXCLUDE_C), $(ARCH_C))
ARCH_S := $(filter-out $(ARCH_EXCLUDE_S), $(ARCH_S))

OBJS := $(KERNEL_C:%.c=$(OBJ_DIR)/%.o) \
	$(ARCH_C:%.c=$(OBJ_DIR)/%.o) $(ARCH_S:%.S=$(OBJ_DIR)/%.o)
# OBJS := $(ARCH_C:%.c=$(OBJ_DIR)/%.o) $(ARCH_S:%.S=$(OBJ_DIR)/%.o)


# Collect all source files for formatting (exclude generated/build files)
# Metadata targets do not consume this list, so avoid invoking POSIX find for them.
ifeq ($(filter compdb tidy tidy-file,$(MAKECMDGOALS)),)
FORMAT_SRC := $(shell find kernel arch include mkfs user test \
                -name '*.c' -o -name '*.h' \
                2>/dev/null)
endif
