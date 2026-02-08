#include "mm/swap.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "console.h"
#include "string.h"
#include "drivers/blockdev.h"

#define MAX_SWAP_DEVICES 4
#define SWAP_PAGE_SIZE 4096

typedef struct {
    int id;
    int active;
    uint8_t *storage;
    size_t size;  
    uint8_t *bitmap;  
} swap_device_t;

static swap_device_t swap_devices[MAX_SWAP_DEVICES];

void swap_init() {
    swap_devices[0].id = 0;
    swap_devices[0].active = 1;
    swap_devices[0].size = 1024;  
    
    swap_devices[0].storage = (uint8_t*)kmalloc(swap_devices[0].size * SWAP_PAGE_SIZE);
    swap_devices[0].bitmap = (uint8_t*)kmalloc(swap_devices[0].size);  
    
    if (!swap_devices[0].storage || !swap_devices[0].bitmap) {
        kprint_str("Swap: Failed to allocate mock storage\n");
        return;
    }
    
    memset(swap_devices[0].bitmap, 0, swap_devices[0].size);
    kprint_str("Swap: Initialized Swap Device 0 (4MB)\n");
}

static int find_free_swap_slot(int dev_id, uint64_t *offset) {
    swap_device_t *dev = &swap_devices[dev_id];
    for (size_t i = 0; i < dev->size; i++) {
        if (dev->bitmap[i] == 0) {
            dev->bitmap[i] = 1;
            *offset = i;
            return 0;
        }
    }
    return -1;  
}

int swap_out(uint64_t phys_addr, swap_entry_t *entry) {
    int dev_id = 0;
    if (!swap_devices[dev_id].active) return -1;
    
    uint64_t offset;
    if (find_free_swap_slot(dev_id, &offset) != 0) {
        kprint_str("Swap: Device full\n");
        return -1;
    }
    
    void *src = (void*)(phys_addr & 0xFFFFFFFFFFFFF000);  
    void *dst = swap_devices[dev_id].storage + (offset * SWAP_PAGE_SIZE);
    
    memcpy(dst, src, SWAP_PAGE_SIZE);
    
    entry->device_id = dev_id;
    entry->offset = offset;
    
    return 0;
}

int swap_in(swap_entry_t entry, uint64_t *phys_addr) {
    if (entry.device_id >= MAX_SWAP_DEVICES || !swap_devices[entry.device_id].active) {
        return -1;
    }
    
    swap_device_t *dev = &swap_devices[entry.device_id];
    if (entry.offset >= dev->size) return -1;
    
    void *new_page = pmm_alloc_page();
    if (!new_page) return -1;  
    
    void *src = dev->storage + (entry.offset * SWAP_PAGE_SIZE);
    memcpy(new_page, src, SWAP_PAGE_SIZE);
    
    dev->bitmap[entry.offset] = 0;
    
    *phys_addr = (uint64_t)new_page;
    return 0;
}

int handle_swap_fault(uint64_t vaddr) {
    uint64_t pte = vmm_get_pte(vaddr);
    
    if ((pte & PTE_PRESENT) || !(pte & PTE_SWAPPED)) {
        return -1;  
    }
    
    swap_entry_t entry;
    entry.device_id = (pte >> 1) & 0xFF;  
    entry.offset = (pte >> 12);           
    
    kprint_str("Swap: Handling page fault at "); kprint_hex(vaddr);
    kprint_str(" (Dev: "); kprint_dec(entry.device_id);
    kprint_str(", Off: "); kprint_dec(entry.offset);
    kprint_str(")\n");
    
    uint64_t phys_addr;
    if (swap_in(entry, &phys_addr) != 0) {
        kprint_str("Swap: Failed to swap in!\n");
        return -1;
    }

    vmm_map_page(vaddr, phys_addr, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    
    return 0;
}
