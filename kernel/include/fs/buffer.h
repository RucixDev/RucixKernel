#ifndef BUFFER_H
#define BUFFER_H

#include "types.h"
#include "list.h"
#include "spinlock.h"
#include "drivers/blockdev.h"
#include "waitqueue.h"
#include "bitops.h"

#define BH_Uptodate    0   
#define BH_Dirty       1   
#define BH_Locked      2   
#define BH_Req         3   
#define BH_Mapped      4   

struct buffer_head {
    unsigned long b_state;           
    struct buffer_head *b_this_page; 
    struct page *b_page;             
    
    char *b_data;                    
    uint64_t b_blocknr;              
    uint32_t b_size;                 
    
    struct gendisk *b_bdev;          
    
    bio_end_io_t b_end_io;           
    void *b_private;                 
    
    struct list_head b_assoc_buffers;  
    
    atomic_t b_count;                
    
    wait_queue_t b_wait;             
    
    struct buffer_head *b_next_hash;
    struct buffer_head **b_pprev_hash;
};

 
void buffer_init(void);
struct buffer_head *getblk(struct gendisk *bdev, uint64_t block, uint32_t size);
struct buffer_head *bread(struct gendisk *bdev, uint64_t block, uint32_t size);
void brelse(struct buffer_head *bh);
int mark_buffer_dirty(struct buffer_head *bh);
int sync_dirty_buffer(struct buffer_head *bh);
void ll_rw_block(int rw, int nr, struct buffer_head *bhs[]);
void lock_buffer(struct buffer_head *bh);
void unlock_buffer(struct buffer_head *bh);

static inline int buffer_uptodate(struct buffer_head *bh) {
    return (bh->b_state & (1 << BH_Uptodate));
}

static inline void set_buffer_uptodate(struct buffer_head *bh) {
    bh->b_state |= (1 << BH_Uptodate);
}

static inline int buffer_dirty(struct buffer_head *bh) {
    return (bh->b_state & (1 << BH_Dirty));
}

static inline int buffer_locked(struct buffer_head *bh) {
    return (bh->b_state & (1 << BH_Locked));
}

void lock_buffer(struct buffer_head *bh);
void unlock_buffer(struct buffer_head *bh);
void wait_on_buffer(struct buffer_head *bh);

#endif
