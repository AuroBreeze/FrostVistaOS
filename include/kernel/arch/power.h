#ifndef FV_KERNEL_ARCH_POWER_H
#define FV_KERNEL_ARCH_POWER_H

/* Power off the machine using the current architecture's firmware/device. */
void arch_shutdown(void) __attribute__((noreturn));

#endif
