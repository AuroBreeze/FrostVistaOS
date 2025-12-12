#ifndef KALLOC_H
#define KALLOC_H

void kalloc_init();
void kfree(void *pa);
void *kalloc();

#endif
