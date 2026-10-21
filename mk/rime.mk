# Shared userspace library objects for tests and applications.
RIME_SRCS := $(sort $(wildcard user/rime/*.c))
RIME_HEADERS := $(wildcard include/user/rime/*.h)
RIME_OBJS := $(patsubst user/rime/%.c,$(USER_DIR)/rime/%.o,$(RIME_SRCS))

$(USER_DIR)/rime/%.o: user/rime/%.c include/user/user.h $(RIME_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@
