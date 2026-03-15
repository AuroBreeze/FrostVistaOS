#include "core/sysfile.h"
#include "driver/hal_console.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "platform/uart.h"

uint64 sys_write(uint64 *regs) {
  LOG_TRACE("sys_write called");
  hal_console_puts("Hello from sys_write\n");
  return 0;
}
