#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include "types.h"
#include "list.h"
#include "spinlock.h"

#define READ 0
#define WRITE 1

#define ELEVATOR_INSERT_BACK  0
#define ELEVATOR_INSERT_FRONT 1
#define ELEVATOR_INSERT_SORT  2

struct bio;
struct request_queue;
struct gendisk;
struct request;

typedef void (*bio_end_io_t)(struct bio *);
typedef void (*make_request_fn)(struct request_queue *q, struct bio *bio);
typedef void (*request_fn_proc)(struct request_queue *q);

struct bio_vec {
    void *page;      
    uint32_t len;    
    uint32_t offset;  
};

struct bio {
    uint64_t sector;
    uint32_t size;       
    
    struct gendisk *disk;  

    struct bio_vec *io_vec;
    uint16_t vc_cnt;     
    uint16_t idx;        

    uint32_t rw;         
    uint32_t flags;

    bio_end_io_t end_io;
    void *private_data;
    struct bio *next;    
};

 
struct request {
    struct list_head queuelist;
    uint64_t sector;
    uint32_t nr_sectors;
    void *buffer;
    struct bio *bio;
    struct request_queue *q;
    int flags;
};

struct elevator_ops {
    int (*elevator_merge_fn)(struct request_queue *, struct request *, struct bio *);
    void (*elevator_merge_req_fn)(struct request_queue *, struct request *, struct request *);
    struct request *(*elevator_next_req_fn)(struct request_queue *);
    void (*elevator_add_req_fn)(struct request_queue *, struct request *, int where);
};

struct elevator_type {
    struct list_head list;
    struct elevator_ops *ops;
    char elevator_name[16];
    void *elevator_owner;
};

struct elevator_queue {
    struct elevator_type *type;
    void *elevator_data;
    struct elevator_ops *ops;
    struct list_head queue_head;
};

struct request_queue {
    struct list_head queue_head;
    request_fn_proc request_fn;
    make_request_fn make_request_fn;
    struct elevator_queue *elevator;
    spinlock_t lock;
    void *queuedata;
};

struct gendisk {
    int major;
    int first_minor;
    int minors;
    char disk_name[32];
    struct request_queue *queue;
    uint64_t capacity;
    void *private_data;
    void *fops;  
    struct list_head list;
};

int register_blkdev(unsigned int major, const char *name);
void unregister_blkdev(unsigned int major, const char *name);
struct gendisk *alloc_disk(int minors);
void add_disk(struct gendisk *disk);
struct gendisk *get_gendisk(const char *name);
void del_gendisk(struct gendisk *disk);
void blockdev_register_devices(void);

struct request_queue *blk_init_queue(request_fn_proc rfn, spinlock_t *lock);
void blk_queue_make_request(struct request_queue *q, make_request_fn mfn);
void submit_bio(struct bio *bio);

int elevator_init(struct request_queue *q, char *name);
void elevator_exit(struct elevator_queue *e);
void __elv_add_request(struct request_queue *q, struct request *rq, int where);
struct request *elv_next_request(struct request_queue *q);
int elv_merge_bio(struct request_queue *q, struct bio *bio);

#endif
