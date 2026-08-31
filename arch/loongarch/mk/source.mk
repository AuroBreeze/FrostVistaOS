# Source discovery and object list generation.
#
# Consumes:
# 	ARCH, ARCH_EXCLUDE_C, ARCH_EXCLUDE_S, OBJ_DIR
# Produces:
# 	KERNEL_C, OBJS, FORMAT_SRC, ARCH_C, ARCH_S

KERNEL_C := $(wildcard kernel/core/*.c)
KERNEL_C += $(wildcard kernel/mm/*.c)
KERNEL_C += $(wildcard kernel/mm/*/*.c)

# Build the common filesystem layer and supported filesystem backends.
# LoongArch bring-up excludes block_cache, EasyFS, and EXT4 for now.
KERNEL_C += $(filter-out kernel/fs/block_cache.c,$(wildcard kernel/fs/*.c))
KERNEL_C += $(filter-out kernel/fs/easyfs/%.c kernel/fs/ext4fs/%.c,\
	$(wildcard kernel/fs/*/*.c))

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
FORMAT_SRC := $(shell find kernel arch include mkfs user test \
                -name '*.c' -o -name '*.h' \
                2>/dev/null)
