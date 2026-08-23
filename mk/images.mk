# Filesystem image and host mkfs rules.
#
# Consumes:
#   HOST_CC, BUILD_DIR, MKFS_TOOL
# Produces:
# 	disk.img, mkfs_tool

MKFS_TOOL = $(BUILD_DIR)/mkfs_tool

disk.img:
	dd if=/dev/zero of=$@ bs=1M count=32

# Generate a 32MB raw disk image and format it with mkfs
$(MKFS_TOOL): mkfs/mkfs.c
	@echo "Building host tool: $(MKFS_TOOL)"
	@mkdir -p $(BUILD_DIR)
	$(HOST_CC) -O2 mkfs/mkfs.c -o $(MKFS_TOOL) -Iinclude

