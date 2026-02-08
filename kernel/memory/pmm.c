#include "pmm.h"
#include "console.h"

extern uint64_t _kernel_end;  

static uint8_t* bitmap __attribute__((section(".data")));
static uint64_t total_pages __attribute__((section(".data"))) = 0;
static uint64_t bitmap_size __attribute__((section(".data"))) = 0;
static uint64_t highest_addr __attribute__((section(".data"))) = 0;
static uint64_t free_memory __attribute__((section(".data"))) = 0;
static uint64_t last_free_index __attribute__((section(".data"))) = 0;  

void pmm_set_bit(uint64_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

void pmm_clear_bit(uint64_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

int pmm_test_bit(uint64_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

uint64_t pmm_get_total_memory() {
    return highest_addr;
}

uint64_t pmm_get_free_memory() {
    return free_memory;
}
 
int64_t pmm_find_first_free() {
     
    for (uint64_t i = last_free_index; i < total_pages; i++) {
        if (!pmm_test_bit(i)) {
            last_free_index = i;
            return i;
        }
    }
    
    for (uint64_t i = 0; i < last_free_index; i++) {
        if (!pmm_test_bit(i)) {
            last_free_index = i;
            return i;
        }
    }
    
    return -1;
}

void pmm_init(uint64_t multiboot_addr, uint64_t magic) {
    kprint_str("Initializing PMM...\n");

    highest_addr = 0;

    if (magic == 0x2BADB002) {
        struct multiboot1_info* mb1 = (struct multiboot1_info*)multiboot_addr;
        if (mb1->flags & 0x40) {
            struct multiboot1_mmap_entry* entry = (struct multiboot1_mmap_entry*)(uint64_t)mb1->mmap_addr;
            uint64_t end_addr = mb1->mmap_addr + mb1->mmap_length;
            while ((uint64_t)entry < end_addr) {
                if (entry->type == 1) {  
                     uint64_t end = entry->addr + entry->len;
                     if (end > highest_addr) highest_addr = end;
                }
                entry = (struct multiboot1_mmap_entry*)((uint64_t)entry + entry->size + 4);
            }
        }
    } else if (magic == 0x36D76289) {
         struct multiboot_tag* tag;
         uint8_t* tag_ptr = (uint8_t*)multiboot_addr + 8;
         uint32_t total_size = *(uint32_t*)multiboot_addr;
         
         while ((uint64_t)tag_ptr < multiboot_addr + total_size) {
             tag = (struct multiboot_tag*)tag_ptr;
             if (tag->type == 0) break;
             if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
                 struct multiboot_tag_mmap* mmap = (struct multiboot_tag_mmap*)tag;
                  
                 uint64_t num_entries = (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size;
                 for (uint64_t i = 0; i < num_entries; i++) {
                     struct multiboot_mmap_entry* e = (struct multiboot_mmap_entry*)((uint64_t)mmap->entries + (i * mmap->entry_size));
                     if (e->type == 1) {
                         uint64_t end = e->addr + e->len;
                         if (end > highest_addr) highest_addr = end;
                     }
                 }
             }
              
             uint32_t size = tag->size;
             if ((size % 8) != 0) size += (8 - (size % 8));
             tag_ptr += size;
         }
    }

    if (highest_addr == 0) {
        kprint_str("Error: Could not detect memory size. Assuming 128MB.\n");
        highest_addr = 128 * 1024 * 1024;
    }

    if (highest_addr > 0x100000000) {
         kprint_str("Warning: Memory > 4GB detected. Capping to 4GB for safety.\n");
         highest_addr = 0x100000000;
    }

    total_pages = highest_addr / PAGE_SIZE;
    bitmap_size = total_pages / 8;

    bitmap = (uint8_t*)PAGE_ALIGN((uint64_t)&_kernel_end);

    uint64_t multiboot_end = 0;
    if (magic == 0x36D76289) {
         uint32_t total_size = *(uint32_t*)multiboot_addr;
         multiboot_end = multiboot_addr + total_size;
    } else if (magic == 0x2BADB002) {

         struct multiboot1_info* mb1 = (struct multiboot1_info*)multiboot_addr;
          
         if ((uint64_t)mb1 > 0x1000 && (uint64_t)mb1 < highest_addr) {
             if ((uint64_t)mb1 >= (uint64_t)bitmap) {
                 multiboot_end = (uint64_t)mb1 + sizeof(struct multiboot1_info);  
                 if (mb1->flags & 0x40) {
                     uint64_t mmap_end = mb1->mmap_addr + mb1->mmap_length;
                     if (mmap_end > multiboot_end) multiboot_end = mmap_end;
                 }
             }
         }
    }

    if (multiboot_end > (uint64_t)bitmap) {
        multiboot_end = PAGE_ALIGN(multiboot_end);
        bitmap = (uint8_t*)multiboot_end;
        kprint_str("Moved Bitmap to: ");
        kprint_hex((uint64_t)bitmap);
        kprint_newline();
    }
    
    if ((uint64_t)bitmap + bitmap_size > 0x40000000) {
        kprint_str("CRITICAL ERROR: Bitmap exceeds identity mapped memory (1GB)!\n");
        while(1);
    }

    kprint_str("PMM Debug: Bitmap @ ");
    kprint_hex((uint64_t)bitmap);
    kprint_str(" Size: ");
    kprint_dec(bitmap_size);
    kprint_str("\n");

    for (uint64_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF; 
    }

    if (magic == 0x2BADB002) {
        kprint_str("PMM: Detected Multiboot 1\n");
        struct multiboot1_info* mb1 = (struct multiboot1_info*)multiboot_addr;
        
        if (mb1->flags & 0x40) {
            kprint_str("PMM: Memory Map present\n");
            struct multiboot1_mmap_entry* entry = (struct multiboot1_mmap_entry*)(uint64_t)mb1->mmap_addr;
            uint64_t end_addr = mb1->mmap_addr + mb1->mmap_length;
            while ((uint64_t)entry < end_addr) {
                if (entry->type == 1) {  
                    uint64_t start_frame = entry->addr / PAGE_SIZE;
                    uint64_t num_frames = entry->len / PAGE_SIZE;
                     
                    for (uint64_t i = 0; i < num_frames; i++) {
                        pmm_clear_bit(start_frame + i);
                    }
                }
                entry = (struct multiboot1_mmap_entry*)((uint64_t)entry + entry->size + 4);
            }
        } else {
             kprint_str("PMM Warning: No memory map flag in Multiboot 1 info!\n");
        }
    } else if (magic == 0x36D76289) {
         
         struct multiboot_tag* tag;
         uint8_t* tag_ptr = (uint8_t*)multiboot_addr + 8;
         uint32_t total_size = *(uint32_t*)multiboot_addr;
         
         while ((uint64_t)tag_ptr < multiboot_addr + total_size) {
             tag = (struct multiboot_tag*)tag_ptr;
             if (tag->type == 0) break;
             if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
                 struct multiboot_tag_mmap* mmap = (struct multiboot_tag_mmap*)tag;
                 uint64_t num_entries = (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size;
                 for (uint64_t i = 0; i < num_entries; i++) {
                     struct multiboot_mmap_entry* e = (struct multiboot_mmap_entry*)((uint64_t)mmap->entries + (i * mmap->entry_size));
                     if (e->type == 1) {
                        uint64_t start_frame = e->addr / PAGE_SIZE;
                        uint64_t num_frames = e->len / PAGE_SIZE;
                        for (uint64_t f = 0; f < num_frames; f++) {
                            if (start_frame + f < total_pages)
                                pmm_clear_bit(start_frame + f);
                        }
                     }
                 }
             }
             uint32_t size = tag->size;
             if ((size % 8) != 0) size += (8 - (size % 8));
             tag_ptr += size;
         }
    }

    uint64_t bitmap_start_p = (uint64_t)bitmap;
    uint64_t bitmap_end_p = bitmap_start_p + bitmap_size;

    uint64_t start_frame = 0; 
    uint64_t end_frame = (bitmap_end_p + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = start_frame; i < end_frame; i++) {
        pmm_set_bit(i);
    }
    
    free_memory = 0;
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!pmm_test_bit(i)) free_memory += PAGE_SIZE;
    }

    if (free_memory == 0 && highest_addr > 0x100000) {
        kprint_str("PMM Warning: No free memory found from map. Using fallback (1MB - End).\n");
         
        uint64_t start_p = 0x100000 / PAGE_SIZE;
        uint64_t end_p = highest_addr / PAGE_SIZE;
        for (uint64_t i = start_p; i < end_p; i++) {
             pmm_clear_bit(i);
        }
         
        free_memory = 0;
        for (uint64_t i = 0; i < total_pages; i++) {
            if (!pmm_test_bit(i)) free_memory += PAGE_SIZE;
        }
    }

    kprint_str("PMM Initialized. Total RAM: ");
    kprint_dec(highest_addr / 1024 / 1024);
    kprint_str(" MB. Free: ");
    kprint_dec(free_memory / 1024 / 1024);
    kprint_str(" MB.\n");

    kprint_str("PMM Debug: Bitmap @ ");
    kprint_hex((uint64_t)bitmap);
    kprint_str(" Size: ");
    kprint_dec(bitmap_size);
    kprint_str("\n");
    kprint_str("Bitmap[0]: ");
    kprint_hex(bitmap[0]);
    kprint_str(" Bitmap[1]: ");
    kprint_hex(bitmap[1]);
    kprint_str("\n");

    uint64_t reserved_end = (uint64_t)bitmap + bitmap_size;
     
    reserved_end = PAGE_ALIGN(reserved_end);
    
    uint64_t reserved_frames = reserved_end / PAGE_SIZE;
    
    kprint_str("PMM: Reserving Kernel+Bitmap (0 - ");
    kprint_hex(reserved_end);
    kprint_str(")\n");
    
    for (uint64_t i = 0; i < reserved_frames; i++) {
        if (i < total_pages) {
            pmm_set_bit(i);
        }
    }

     
    pmm_set_bit(0);
}

void* pmm_alloc_page() {
    int64_t bit = pmm_find_first_free();

    if (bit == -1) {
        kprint_str("PMM Alloc Error: No free pages! Total: ");
        kprint_dec(total_pages);
        kprint_str(" Free: ");
        kprint_dec(free_memory);
        kprint_newline();
        return 0;
    }

    if (bit == 0) {
         
        pmm_set_bit(0);
        return pmm_alloc_page();  
    }

    pmm_set_bit(bit);
    free_memory -= PAGE_SIZE;
    
     
    if ((uint64_t)(bit * PAGE_SIZE) >= 0xC0000000) {
        kprint_str("PMM Alloc Warning: Allocated > 3GB. Might fault if not mapped.\n");
    }
    
    return (void*)(bit * PAGE_SIZE);
}

void* pmm_alloc_pages(uint64_t count) {
    if (count == 0) return 0;
    
     
    for (uint64_t i = 0; i <= total_pages - count; i++) {
        int found = 1;
        for (uint64_t j = 0; j < count; j++) {
            if (pmm_test_bit(i + j)) {
                found = 0;
                i += j;  
                break;
            }
        }
        
        if (found) {
            for (uint64_t j = 0; j < count; j++) {
                pmm_set_bit(i + j);
            }
            free_memory -= (count * PAGE_SIZE);
            return (void*)(i * PAGE_SIZE);
        }
    }
    return 0;
}

void pmm_free_page(void* addr) {
    uint64_t bit = (uint64_t)addr / PAGE_SIZE;
    if (bit < total_pages) {
        pmm_clear_bit(bit);
        free_memory += PAGE_SIZE;
         
        if (bit < last_free_index) {
            last_free_index = bit;
        }
    }
}

void pmm_free_pages(void* addr, uint64_t count) {
    uint64_t start_bit = (uint64_t)addr / PAGE_SIZE;
    for (uint64_t i = 0; i < count; i++) {
        if (start_bit + i < total_pages) {
            pmm_clear_bit(start_bit + i);
        }
    }
    free_memory += (count * PAGE_SIZE);
     
    if (start_bit < last_free_index) {
        last_free_index = start_bit;
    }
}
