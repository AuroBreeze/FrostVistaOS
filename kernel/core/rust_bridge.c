
#define LOG_MODULE "RUST_BRIDGE"

#include "kernel/arch/cpu.h"
#include "kernel/defs.h"
#include "kernel/types.h"

void fv_console_write(const char *data)
{
	kprintf("%s", data);
};

void fv_log_write(uint32 level, const char *data)
{
	(void) level;

	if (data == 0)
		return;

	kprintf("%s", data);
}

__attribute__((noreturn)) void fv_panic_write(const char *data)
{
	_panic("rust", 0, "%s", data);

	while (1) {
		arch_cpu_wait();
	};
}

void *fv_page_alloc(void)
{
	return kalloc();
}

void fv_page_free(void *kva)
{
	kfree(kva);
}
