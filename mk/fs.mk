# Filesystem feature and root filesystem selection.
#
# Consumes:
#   FS_LIST, ROOTFS, DISK_IMG, EXT4_IMG
#
# Produces:
#   FS_CFLAGS, ROOTFS_CFLAGS, ROOTFS_IMG, ROOTFS_DEPS

# Select the boot-time root filesystem path.
# easyfs keeps the local generated disk workflow; ext4 uses the contest image.

KNOWN_FS := easyfs ext4 devtmpfs
UNKNOWN_FS := $(filter-out $(KNOWN_FS),$(FS_LIST))

ifneq ($(UNKNOWN_FS),)
  $(error Unsupported FS_LIST entries: $(UNKNOWN_FS). Known: $(KNOWN_FS))
endif

ifneq ($(filter $(ROOTFS),$(FS_LIST)),)
else
  $(error ROOTFS=$(ROOTFS) must be included in FS_LIST=$(FS_LIST))
endif

ifneq ($(filter easyfs,$(FS_LIST)),)
  FS_CFLAGS += -DCONFIG_FS_EASYFS
endif

ifneq ($(filter ext4,$(FS_LIST)),)
  FS_CFLAGS += -DCONFIG_FS_EXT4
endif

ifneq ($(filter devtmpfs,$(FS_LIST)),)
  FS_CFLAGS += -DCONFIG_FS_DEVTMPFS
endif

ifeq ($(FS), easyfs)
  ROOTFS_CFLAGS := -DROOTFS_EASYFS
  ROOTFS_IMG := $(DISK_IMG)
  ROOTFS_DEPS := $(DISK_IMG)
else ifeq ($(FS), ext4)
  ROOTFS_CFLAGS := -DROOTFS_EXT4
  ROOTFS_IMG := $(EXT4_IMG)
  ROOTFS_DEPS :=
else
  $(error Unsupported FS=$(FS). Use FS=easyfs or FS=ext4)
endif
