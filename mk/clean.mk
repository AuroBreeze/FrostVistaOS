# Clean targets.
#
# Consumes:
#   BUILD_DIR, DISK_IMG
#
# Produces targets:
#   clean, clean_disk

# Separate target to clean the disk image, preventing accidental data loss
clean_disk:
	@echo "Cleaning disk image: $(DISK_IMG)"
	rm -f $(DISK_IMG)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf kernel-rv
