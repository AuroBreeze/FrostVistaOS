# Kernel, user test, and object build rules.
#
# Consumes:
# 	GEN_DIR, CC, LDFLAGS , OBJS, LINKER_SCRIPT, ARCH
# 	OPT_FLAGS, LOG_NUM, CONFIG_TEST
#
# Produces:
# 	kernel.elf

INCLUDES = -I$(GEN_DIR) -Iinclude -Iarch/$(ARCH)/include
LDFLAGS = -T $(LINKER_SCRIPT)


CFLAGS = $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding -fno-pie $(OPT_FLAGS) $(INCLUDES)
CFLAGS += -fno-unwind-tables -fno-asynchronous-unwind-tables
CFLAGS += $(FS_CFLAGS) $(ROOTFS_CFLAGS)
# if log
CFLAGS += -DCURRENT_LOG_LEVEL=$(LOG_NUM)

USER_CFLAGS = $(ARCH_CFLAGS) -nostdlib -fno-builtin -ffreestanding \
	-Iuser -Itest -Iarch/$(ARCH)/include $(OPT_FLAGS)

# Avoid a build-id LOAD segment at the linker's default high user address.
USER_LINKER_SCRIPT = arch/$(ARCH)/user/user_linker.ld
USER_LDFLAGS = -static \
	-Wl,-T,$(USER_LINKER_SCRIPT) \
	-Wl,--build-id=none \
	-Wl,-z,max-page-size=0x1000

# Build and install the same user applications as the RISC-V image.
USER_APP_SRCS = $(wildcard user/bin/*.c)
USER_APP_NAMES = $(basename $(notdir $(USER_APP_SRCS)))
USER_APP_BINS = $(addprefix $(USER_DIR)/,$(USER_APP_NAMES))
USER_FS_ENTRIES = $(foreach app,$(USER_APP_NAMES),$(USER_DIR)/$(app):$(app))

ifeq ($(CONFIG_TEST),Y)
	 CFLAGS += -DCONFIG_TEST
endif

ifeq ($(CONFIG_VIRTIO_TRANSPORT),virtio-mmio)
	CFLAGS += -DVIRTIO_MMIO_TRANSPORT
else ifeq ($(CONFIG_VIRTIO_TRANSPORT),virtio-pci)
	CFLAGS += -DVIRTIO_PCI_TRANSPORT
endif

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS) $(LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(CROSS)-ld $(OBJS) $(RUST_LIB) $(LDFLAGS) -o $@

build_test:
	@echo "Building user test: test/test_$(TEST).c"
	@mkdir -p $(TEST_DIR)
	$(CC) $(USER_CFLAGS) -c user/ulib.c -o $(TEST_DIR)/ulib.o
	$(CC) $(USER_CFLAGS) -c arch/$(ARCH)/user/__restore.S -o $(TEST_DIR)/restore.o
	$(CC) $(USER_CFLAGS) -c test/test_$(TEST).c -o $(TEST_DIR)/test.o
	$(CC) $(USER_CFLAGS) $(USER_LDFLAGS) $(TEST_DIR)/ulib.o \
		$(TEST_DIR)/restore.o $(TEST_DIR)/test.o -o $(TEST_DIR)/init_bin
	@echo "Generated $(TEST_DIR)/init_bin"
	@echo "Embedding $(TEST_DIR)/init_bin as /init via $(GEN_DIR)/kernel/init_code.h"
	@mkdir -p $(GEN_DIR)/kernel
	@if command -v $(XXD) >/dev/null 2>&1; then \
		$(XXD) -i -n init_code $(TEST_DIR)/init_bin > $(GEN_DIR)/kernel/init_code.h; \
	else \
		{ \
			echo 'unsigned char init_code[] = {'; \
			od -An -v -tx1 $(TEST_DIR)/init_bin | awk '{ for (i = 1; i <= NF; i++) printf "  0x%s,\n", $$i }'; \
			echo '};'; \
			bytes=$$(wc -c < $(TEST_DIR)/init_bin); \
			printf 'unsigned int init_code_len = %s;\n' "$$bytes"; \
		} > $(GEN_DIR)/kernel/init_code.h; \
	fi
	@echo "Generated $(GEN_DIR)/kernel/init_code.h"



$(USER_DIR)/restore.o: arch/$(ARCH)/user/__restore.S
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/ulib.o: user/ulib.c user/user.h arch/$(ARCH)/include/asm/user.h
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/%: user/bin/%.c user/bin/user.h user/user.h $(USER_DIR)/ulib.o $(USER_DIR)/restore.o $(USER_LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $(USER_DIR)/$*.o
	$(CC) $(USER_CFLAGS) $(USER_LDFLAGS) $(USER_DIR)/ulib.o \
		$(USER_DIR)/restore.o $(USER_DIR)/$*.o -o $@

.PHONY: build_user_apps
build_user_apps: $(USER_APP_BINS)
	@echo "Generated user apps: $(USER_APP_BINS)"

.PHONY: check-direct-boot

check-direct-boot: $(BUILD_DIR)/kernel.elf
	sh scripts/check_loongarch_direct_boot.sh $< $(CROSS)-readelf
