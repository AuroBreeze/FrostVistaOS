# Filesystem image and host mkfs rules.
#
# Consumes:
#   HOST_CC, BUILD_DIR, MKFS_TOOL, DISK_IMG, TEST_DIR
#
# Produces targets:
#   $(MKFS_TOOL), $(DISK_IMG), disk.img

MKFS_TOOL = $(BUILD_DIR)/mkfs_tool

disk.img:
	dd if=/dev/zero of=$@ bs=1M count=32

# Generate a 32MB raw disk image and format it with mkfs
$(MKFS_TOOL): mkfs/mkfs.c
	@echo "Building host tool: $(MKFS_TOOL)"
	@mkdir -p $(BUILD_DIR)
	$(HOST_CC) -O2 mkfs/mkfs.c -o $(MKFS_TOOL) -Iinclude

$(DISK_IMG): $(MKFS_TOOL) build_test clean_disk
	@echo "Generating empty disk image: $@"
	dd if=/dev/zero of=$@ bs=1M count=32

	@echo "Formatting the disk image with your filesystem..."
	# Run the formatting tool on the freshly zeroed disk
	./$(MKFS_TOOL) $@ $(TEST_DIR)/init_bin
