# Source discovery and object list generation.
#
# Consumes:
#   ARCH, FS_LIST, OBJ_DIR, ARCH_EXCLUDE_C, ARCH_EXCLUDE_S
#
# Produces:
#   KERNEL_C, ARCH_C, ARCH_S, OBJS, FORMAT_SRC

# Obtain all common kernel C files
KERNEL_C := $(wildcard kernel/*/*.c)

# fs/*.c (such as easyfs)
KERNEL_C += $(wildcard kernel/fs/*/*.c)

# Get all architecture specific C/S files
ARCH_C := $(wildcard arch/$(ARCH)/*/*.c)
ARCH_S := $(wildcard arch/$(ARCH)/*/*.S)


ARCH_C := $(filter-out $(ARCH_EXCLUDE_C), $(ARCH_C))
ARCH_S := $(filter-out $(ARCH_EXCLUDE_S), $(ARCH_S))

OBJS := $(KERNEL_C:%.c=$(OBJ_DIR)/%.o) $(ARCH_C:%.c=$(OBJ_DIR)/%.o) $(ARCH_S:%.S=$(OBJ_DIR)/%.o)

# Collect all source files for formatting (exclude generated/build files)
FORMAT_SRC := $(shell find kernel arch include mkfs test \
                -name '*.c' -o -name '*.h' \
                2>/dev/null)


