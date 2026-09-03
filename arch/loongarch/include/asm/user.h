#ifndef __LOONGARCH_USER_H
#define __LOONGARCH_USER_H

/* LoongArch Linux ABI: a0-a5 carry arguments and a7 carries the syscall id. */
static inline long arch_user_syscall3(long num, long a0, long a1, long a2)
{
	register long a0_asm __asm__("a0") = a0;
	register long a1_asm __asm__("a1") = a1;
	register long a2_asm __asm__("a2") = a2;
	register long a7_asm __asm__("a7") = num;
	__asm__ volatile("syscall 0"
			 : "+r"(a0_asm)
			 : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm)
			 : "memory");
	return a0_asm;
}

static inline long arch_user_syscall4(long num, long a0, long a1, long a2,
					      long a3)
{
	register long a0_asm __asm__("a0") = a0;
	register long a1_asm __asm__("a1") = a1;
	register long a2_asm __asm__("a2") = a2;
	register long a3_asm __asm__("a3") = a3;
	register long a7_asm __asm__("a7") = num;
	__asm__ volatile("syscall 0"
			 : "+r"(a0_asm)
			 : "r"(a1_asm), "r"(a2_asm), "r"(a3_asm), "r"(a7_asm)
			 : "memory");
	return a0_asm;
}

static inline long arch_user_syscall6(long num, long a0, long a1, long a2,
					      long a3, long a4, long a5)
{
	register long a0_asm __asm__("a0") = a0;
	register long a1_asm __asm__("a1") = a1;
	register long a2_asm __asm__("a2") = a2;
	register long a3_asm __asm__("a3") = a3;
	register long a4_asm __asm__("a4") = a4;
	register long a5_asm __asm__("a5") = a5;
	register long a7_asm __asm__("a7") = num;
	__asm__ volatile("syscall 0"
			 : "+r"(a0_asm)
			 : "r"(a1_asm), "r"(a2_asm), "r"(a3_asm), "r"(a4_asm),
			   "r"(a5_asm), "r"(a7_asm)
			 : "memory");
	return a0_asm;
}

#endif
