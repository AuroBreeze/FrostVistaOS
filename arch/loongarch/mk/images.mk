
$(DISK_IMG): $(MKFS_TOOL) build_test
	@mkdir -p $(dir $@)
	@echo "Generating empty disk image: $@"
	dd if=/dev/zero of=$@ bs=1M count=32
	./$(MKFS_TOOL) $@ $(TEST_DIR)/init_bin:init
