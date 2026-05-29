# Filesystem feature and root filesystem selection.
#
# Consumes:
#   FS_LIST, ROOTFS, DISK_IMG, EXT4_IMG
#
# Produces:
#   FS_CFLAGS, ROOTFS_CFLAGS, ROOTFS_IMG, ROOTFS_DEPS

# Select the boot-time root filesystem path.
# easyfs keeps the local generated disk workflow; ext4 uses the contest image.

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
