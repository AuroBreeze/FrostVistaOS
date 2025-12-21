#ifndef MM_H
#define MM_H

#include "kernel/types.h"
// dirty bit that to use write back
#define PTE_D (1 << 7)
// Whether to visit
#define PTE_A (1 << 6)

#define PTE_G (1 << 5)
// U mode can access
#define PTE_U (1 << 4)
// allow perfom
#define PTE_X (1 << 3)
// allow write
#define PTE_W (1 << 2)
// allow read
#define PTE_R (1 << 1)
// persent vaild
#define PTE_V (1 << 0)

#define VPN_MASK 0x1ff // Obtain the required VPN subnet mask
#define VPN_BITS 9     // Number of positions occupied by VA VPN
#define ADDR_PF 12 // Page Offset Between Virtual Address and Physical Address

// Get VPN for VA
#define VPN_GET(va, i) (((uint64)va >> (ADDR_PF + (VPN_BITS * i))) & VPN_MASK)

#define PTE2PA(pte) ((pte >> 10) << ADDR_PF)
#define PA2PTE(pa) (((uint64)pa >> ADDR_PF) << 10)

#define PTE_FLAGS(pte) (pte & 0x3ff)

#endif
