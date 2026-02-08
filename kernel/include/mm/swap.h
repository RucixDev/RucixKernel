#ifndef SWAP_H
#define SWAP_H

#include <stdint.h>
#include <stddef.h>

 
#define PTE_SWAPPED 0x200  

typedef struct {
    uint64_t offset;  
    uint8_t device_id;
} swap_entry_t;

void swap_init();
int swap_out(uint64_t phys_addr, swap_entry_t *entry);
int swap_in(swap_entry_t entry, uint64_t *phys_addr);

 
int handle_swap_fault(uint64_t vaddr);

#endif
