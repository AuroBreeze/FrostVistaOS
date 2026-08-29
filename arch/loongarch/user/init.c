#include "kernel/arch/user.h"

/*
 * LoongArch user-mode syscall entry is not implemented yet.  Returning an
 * empty image prevents the RISC-V bootstrap program from being executed on
 * LoongArch while keeping the common process code architecture-neutral.
 */
const uint8 *arch_user_init_code(void)
{
	return 0;
}

uint64 arch_user_init_code_size(void)
{
	return 0;
}
