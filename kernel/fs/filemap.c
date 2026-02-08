#include "vfs.h"
#include "mm/page.h"
#include "mm/page_cache.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "pmm.h"

int generic_file_read(struct file *filp, char *buf, int count, uint64_t *ppos) {
    struct inode *inode = filp->f_dentry->d_inode;
    struct address_space *mapping = inode->i_mapping;
    
    if (!mapping) return -1;
    
    int read = 0;
    uint64_t pos = *ppos;
    
    while (count > 0) {
        uint64_t page_index = pos >> 12;
        uint32_t offset = pos & 4095;
        uint32_t bytes = 4096 - offset;
        if (bytes > (uint32_t)count) bytes = count;
        
        struct page *page = find_get_page(mapping, page_index);
        if (!page) {
            void *phys = pmm_alloc_page();
            if (!phys) return -1;
            
            page = (struct page*)kmalloc(sizeof(struct page));
            if (!page) {
                pmm_free_page(phys);
                return -1;
            }
            memset(page, 0, sizeof(struct page));
            page->virtual = (void*)((uint64_t)phys);
            
            if (add_to_page_cache(page, mapping, page_index, 0) != 0) {
                kfree(page);
                pmm_free_page(phys);
                page = find_get_page(mapping, page_index);
                if (!page) return -1;
            } else {
                if (mapping->a_ops && mapping->a_ops->readpage) {
                    int ret = mapping->a_ops->readpage(filp, page);
                    if (ret != 0) {
                        return -1;
                    }
                } else {
                    memset(page->virtual, 0, 4096);
                }
            }
        }
        
        char *src = (char*)page->virtual + offset;
        memcpy(buf + read, src, bytes);
        
        put_page(page);
        
        read += bytes;
        pos += bytes;
        count -= bytes;
    }
    
    *ppos = pos;
    return read;
}

 
int generic_file_write(struct file *filp, const char *buf, int count, uint64_t *ppos) {
    struct inode *inode = filp->f_dentry->d_inode;
    struct address_space *mapping = inode->i_mapping;
    
    if (!mapping) return -1;
    
    int written = 0;
    uint64_t pos = *ppos;
    
    while (count > 0) {
        uint64_t page_index = pos >> 12;
        uint32_t offset = pos & 4095;
        uint32_t bytes = 4096 - offset;
        if (bytes > (uint32_t)count) bytes = count;
        
        struct page *page = find_get_page(mapping, page_index);
        if (!page) {
            void *phys = pmm_alloc_page();
            if (!phys) return -1;
            
            page = (struct page*)kmalloc(sizeof(struct page));
            if (!page) {
                pmm_free_page(phys);
                return -1;
            }
            memset(page, 0, sizeof(struct page));
            page->virtual = (void*)((uint64_t)phys);
            
            if (add_to_page_cache(page, mapping, page_index, 0) != 0) {
                kfree(page);
                pmm_free_page(phys);
                page = find_get_page(mapping, page_index);
                if (!page) return -1;
            } else {
                if (bytes < 4096) {  
                     if (mapping->a_ops && mapping->a_ops->readpage) {
                        mapping->a_ops->readpage(filp, page);
                     } else {
                        memset(page->virtual, 0, 4096);
                     }
                }
            }
        }
        
        char *dst = (char*)page->virtual + offset;
        memcpy(dst, buf + written, bytes);
        
        SetPageDirty(page);
        if (pos + bytes > inode->i_size) {
            inode->i_size = pos + bytes;
        }
        
        put_page(page);
        
        written += bytes;
        pos += bytes;
        count -= bytes;
    }
    
    *ppos = pos;
    return written;
}
