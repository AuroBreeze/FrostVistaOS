#ifndef FV_KERNEL_ARCH_USER_H
#define FV_KERNEL_ARCH_USER_H

#include "kernel/types.h"

/* Return the architecture-specific initial user image. */
const uint8 *arch_user_init_code(void);
uint64 arch_user_init_code_size(void);

#endif
