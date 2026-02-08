#ifndef RTMUTEX_H
#define RTMUTEX_H

#include "types.h"
#include "process.h"
#include "spinlock.h"
#include "list.h"

 
typedef struct rt_mutex {
    spinlock_t wait_lock;
    struct process *owner;
    struct list_head wait_list;  
    struct list_head held_list_entry;  
    int save_state;
} rt_mutex_t;

struct rt_mutex_waiter {
    struct list_head list;
    struct process *task;
    struct rt_mutex *lock;
};

void rt_mutex_init(rt_mutex_t *lock);
void rt_mutex_lock(rt_mutex_t *lock);
int rt_mutex_trylock(rt_mutex_t *lock);
void rt_mutex_unlock(rt_mutex_t *lock);

 
void rt_mutex_adjust_prio(struct process *task);

#endif
