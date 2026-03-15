#ifndef SBI_H
#define SBI_H

#include "kernel/types.h"

#define SBI_SUCCESS 0

#define SBI_EID_TIME 0x54494D45L // 'TIME'
#define SBI_FID_SET_TIMER 0
#define SBI_EID_LEGACY_SET_TIMER 0 // legacy

// sbi.c
void sbi_set_timer(uint64 stime_value);
void timer_handle();
#endif
