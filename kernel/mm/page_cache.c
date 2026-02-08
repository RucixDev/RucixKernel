#include "mm/page_cache.h"
#include "heap.h"
#include "string.h"
#include "radix-tree.h"

struct page *find_get_page(struct address_space *mapping, unsigned long offset) {
    struct page *page;

    if (!mapping) return 0;

    spinlock_acquire(&mapping->lock);
    page = radix_tree_lookup(&mapping->page_tree, offset);
    if (page) {
        get_page(page);
    }
    spinlock_release(&mapping->lock);
    return page;
}

int add_to_page_cache(struct page *page, struct address_space *mapping, unsigned long offset, int gfp_mask) {
    (void)gfp_mask;
    if (!mapping || !page) return -1;

    spinlock_acquire(&mapping->lock);
    int ret = radix_tree_insert(&mapping->page_tree, offset, page);
    if (ret == 0) {
        page->mapping = mapping;
        page->index = offset;
        mapping->nrpages++;
    }
    spinlock_release(&mapping->lock);
    return ret;
}

void delete_from_page_cache(struct page *page) {
    struct address_space *mapping = page->mapping;
    if (!mapping) return;

    spinlock_acquire(&mapping->lock);
    if (radix_tree_delete(&mapping->page_tree, page->index)) {
        mapping->nrpages--;
    }
    page->mapping = 0;
    spinlock_release(&mapping->lock);
    
    put_page(page);
}

 
struct page *alloc_page(int flags) {
    (void)flags;
    struct page *p = (struct page *)kmalloc(sizeof(struct page));
    if (!p) return 0;
    memset(p, 0, sizeof(struct page));
    p->_count = 1;
     
    p->virtual = kmalloc(4096); 
    if (!p->virtual) {
        kfree(p);
        return 0;
    }
    memset(p->virtual, 0, 4096);
    return p;
}

void __free_page(struct page *page) {
    if (page->virtual) kfree(page->virtual);
    kfree(page);
}
