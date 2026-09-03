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
CFLAGS += $(FS_CFLAGS)
# if log
CFLAGS += -DCURRENT_LOG_LEVEL=$(LOG_NUM)

USER_CFLAGS = $(ARCH_CFLAGS) -nostdlib -fno-builtin -ffreestanding \
	-Iuser -Itest -Iarch/$(ARCH)/include $(OPT_FLAGS)
USER_LDFLAGS = -e _start -Ttext 0x10000

ifeq ($(CONFIG_TEST),Y)
	 CFLAGS += -DCONFIG_TEST
endif


$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS) $(LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(CROSS)-ld $(OBJS) $(LDFLAGS) -o $@

build_test:
	@echo "Building user test: test/test_$(TEST).c"
	@mkdir -p $(TEST_DIR)
	$(CC) $(USER_CFLAGS) -c user/ulib.c -o $(TEST_DIR)/ulib.o
	$(CC) $(USER_CFLAGS) -c test/test_$(TEST).c -o $(TEST_DIR)/test.o
	$(CC) $(USER_CFLAGS) $(USER_LDFLAGS) $(TEST_DIR)/ulib.o \
		 $(TEST_DIR)/test.o -o $(TEST_DIR)/init_bin
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



.PHONY: check-direct-boot

check-direct-boot: $(BUILD_DIR)/kernel.elf
	sh scripts/check_loongarch_direct_boot.sh $< $(CROSS)-readelf
