# QEMU run profiles and debugger entry points.
#
# Consumes:
# 	CROSS, BUILD_DIR

# Connect GDB to a waiting QEMU
GDB_PORT ?= 1234

gdb:
	$(CROSS)-gdb $(BUILD_DIR)/kernel.elf \
		-ex 'set confirm off' \
		-ex 'target remote :$(GDB_PORT)'
