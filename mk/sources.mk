# Source discovery and object list generation.
#
# Consumes:
#   ARCH, FS_LIST, OBJ_DIR, ARCH_EXCLUDE_C, ARCH_EXCLUDE_S
#
# Produces:
#   KERNEL_C, ARCH_C, ARCH_S, OBJS, FORMAT_SRC

# Obtain all common kernel C files
KERNEL_C := $(wildcard kernel/core/*.c)
KERNEL_C += $(wildcard kernel/driver/*.c)
KERNEL_C += $(wildcard kernel/mm/*.c)
# Common VFS/block layer, always compiled.
KERNEL_C += kernel/fs/block_cache.c
KERNEL_C += kernel/fs/fs.c
KERNEL_C += kernel/fs/inode_cache.c
KERNEL_C += kernel/fs/vfs.c

ifneq ($(filter easyfs,$(FS_LIST)),)
  KERNEL_C += $(wildcard kernel/fs/easyfs/*.c)
endif

ifneq ($(filter ext4,$(FS_LIST)),)
  KERNEL_C += $(wildcard kernel/fs/ext4fs/*.c)
endif

ifneq ($(filter devtmpfs,$(FS_LIST)),)
  KERNEL_C += $(wildcard kernel/fs/devtmpfs/*.c)
endif

ifneq ($(filter tmpfs,$(FS_LIST)),)
  KERNEL_C += $(wildcard kernel/fs/tmpfs/*.c)
endif

# Get all architecture specific C/S files
ARCH_C := $(wildcard arch/$(ARCH)/*/*.c)
ARCH_S := $(wildcard arch/$(ARCH)/*/*.S)


ARCH_C := $(filter-out $(ARCH_EXCLUDE_C), $(ARCH_C))
ARCH_S := $(filter-out $(ARCH_EXCLUDE_S), $(ARCH_S))

OBJS := $(KERNEL_C:%.c=$(OBJ_DIR)/%.o) $(ARCH_C:%.c=$(OBJ_DIR)/%.o) $(ARCH_S:%.S=$(OBJ_DIR)/%.o)

# Collect all source files for formatting (exclude generated/build files)
FORMAT_SRC := $(shell find kernel arch include mkfs user test \
                -name '*.c' -o -name '*.h' \
                2>/dev/null)

