#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "multiboot.h"

#define PAGE_SIZE 4096
#define PAGE_ALIGN(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

void pmm_init(uint64_t multiboot_addr, uint64_t magic);
void* pmm_alloc_page();
void* pmm_alloc_pages(uint64_t count);
void pmm_free_page(void* addr);
void pmm_free_pages(void* addr, uint64_t count);
uint64_t pmm_get_total_memory();
uint64_t pmm_get_free_memory();

#endif
