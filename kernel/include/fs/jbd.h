#ifndef JBD_H
#define JBD_H

#include "types.h"
#include "list.h"
#include "spinlock.h"
#include "fs/buffer.h"

#define JBD_MAGIC_NUMBER 0xC03B3998U

struct journal_superblock {
    uint32_t s_header_size;
    uint32_t s_blocksize;
    uint32_t s_maxlen;
    uint32_t s_first;
    uint32_t s_sequence;
    uint32_t s_start;
    uint32_t s_errno;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    uint32_t s_nr_users;
    uint32_t s_dynsuper;
    uint32_t s_max_transaction;
    uint32_t s_max_trans_data;
    uint8_t  s_padding[1024];
};

struct transaction {
    int t_id;
    int t_state;
    struct list_head t_buffers;  
    struct list_head t_inode_list;
    spinlock_t t_handle_lock;
    unsigned long t_expires;
};

#define T_RUNNING 1
#define T_LOCKED  2
#define T_FLUSH   3
#define T_COMMIT  4
#define T_FINISHED 5

struct handle {
    int h_transaction_id;
    int h_buffer_credits;
    struct transaction *h_transaction;
};

struct journal {
    unsigned long j_flags;
    struct buffer_head *j_sb_buffer;
    struct journal_superblock *j_superblock;
    int j_format_version;
    
     
    struct transaction *j_running_transaction;
    struct transaction *j_committing_transaction;
    struct list_head j_checkpoint_transactions;
    
    unsigned long j_head;
    unsigned long j_tail;
    unsigned long j_free;
    unsigned long j_first;
    unsigned long j_last;
    
    struct block_device *j_dev;
    struct inode *j_inode;
    
    spinlock_t j_state_lock;
};


struct journal *journal_init_dev(struct block_device *bdev, struct block_device *fs_dev,
                                int start, int len, int blocksize);
struct journal *journal_init_inode(struct inode *inode);
int journal_destroy(struct journal *journal);

struct handle *journal_start(struct journal *journal, int nblocks);
int journal_stop(struct handle *handle);
int journal_get_write_access(struct handle *handle, struct buffer_head *bh);
int journal_get_create_access(struct handle *handle, struct buffer_head *bh);
int journal_dirty_metadata(struct handle *handle, struct buffer_head *bh);

#endif
