#ifndef TOOL_H
#define TOOL_H

#include "kernel/types.h"
extern int early_mode;

// tool.c
uint64 next_pc(uint64);
uint64 pt_alloc_page_pa();

#endif
