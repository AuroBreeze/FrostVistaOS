#ifndef MM_H
#define MM_H

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

#endif
