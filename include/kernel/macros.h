#ifndef FV_KERNEL_MACROS_H
#define FV_KERNEL_MACROS_H

/* alignment must be a power of two. */
#define ALIGN_UP(value, alignment)                                             \
	(((value) + (alignment) - 1) & ~((alignment) - 1))

#define ALIGN_DOWN(value, alignment) ((value) & ~((alignment) - 1))

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#endif
