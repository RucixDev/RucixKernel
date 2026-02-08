#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define PTE_PRESENT 1
#define PTE_WRITABLE 2
#define PTE_USER 4
#define PTE_NO_EXEC 0x8000000000000000

void vmm_init();
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);
uint64_t vmm_get_pte(uint64_t virt);
void vmm_free_user_space();
void vmm_dump_stats();

 
void *ioremap(uint64_t phys_addr, uint64_t size);
void iounmap(void *virt_addr);

 
int vmm_swap_out_victim();

#endif
