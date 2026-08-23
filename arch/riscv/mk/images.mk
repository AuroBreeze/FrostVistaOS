$(DISK_IMG): $(MKFS_TOOL) build_test build_user_apps clean_disk
	@echo "Generating empty disk image: $@"
	dd if=/dev/zero of=$@ bs=1M count=32

	@echo "Formatting the disk image with your filesystem..."
	# Run the formatting tool on the freshly zeroed disk
	./$(MKFS_TOOL) $@ $(TEST_DIR)/init_bin:init $(USER_FS_ENTRIES)
