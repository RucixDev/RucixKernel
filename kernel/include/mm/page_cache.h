#ifndef PAGE_CACHE_H
#define PAGE_CACHE_H

#include "mm/page.h"
#include "vfs.h"

struct page *find_get_page(struct address_space *mapping, unsigned long offset);
int add_to_page_cache(struct page *page, struct address_space *mapping, unsigned long offset, int gfp_mask);
void delete_from_page_cache(struct page *page);
struct page *alloc_page(int flags);
void __free_page(struct page *page);

#endif
