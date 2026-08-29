
$(DISK_IMG): $(MKFS_TOOL) clean_disk
	@echo "Generating empty disk image: $@"
	dd if=/dev/zero of=$@ bs=1M count=32
