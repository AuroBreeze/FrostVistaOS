#ifndef CLINT_H
#define CLINT_H

#define CLINT_BASE 0x02000000UL

// Each core(Hart) has one. with an offset of 0x4000
#define CLINT_MTIMECMP(hartid) (CLINT_BASE + 0x4000 + 8 * (hartid))

// mtime register: current time
// All core share one, with an offset of 0xBFF8
#define CLINT_MTIME (CLINT_BASE + 0xBFF8)

#endif
