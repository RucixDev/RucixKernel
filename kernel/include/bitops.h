#ifndef BITOPS_H
#define BITOPS_H

#include <stdint.h>

static inline int test_and_set_bit(int nr, volatile unsigned long *addr) {
    int oldbit;
    asm volatile("lock; bts %2,%1\n\t"
                 "sbb %0,%0"
                 : "=r" (oldbit), "+m" (*addr)
                 : "Ir" (nr) : "memory");
    return oldbit;
}

static inline void clear_bit(int nr, volatile unsigned long *addr) {
    asm volatile("lock; btr %1,%0"
                 : "+m" (*addr)
                 : "Ir" (nr) : "memory");
}

static inline int test_bit(int nr, volatile unsigned long *addr) {
    int oldbit;
    asm volatile("bt %2,%1\n\t"
                 "sbb %0,%0"
                 : "=r" (oldbit)
                 : "m" (*addr), "Ir" (nr));
    return oldbit;
}

#endif
