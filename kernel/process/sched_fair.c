#include "process.h"
#include "console.h"
#include "string.h"
#include "heap.h"

 
struct process* cfs_queue = 0;

static void enqueue_task_fair(struct process *p) {
    p->state = PROCESS_STATE_READY;
    
     
    struct process** curr = &cfs_queue;
    while (*curr && (*curr)->vruntime <= p->vruntime) {
        curr = &(*curr)->next_ready;
    }
    p->next_ready = *curr;
    *curr = p;
}

static void dequeue_task_fair(struct process *p) {
     
    struct process** curr = &cfs_queue;
    while (*curr) {
        if (*curr == p) {
            *curr = p->next_ready;
            p->next_ready = 0;
            return;
        }
        curr = &(*curr)->next_ready;
    }
}

static struct process *pick_next_task_fair(struct process *prev) {
    (void)prev;
    if (!cfs_queue) return 0;
    
    struct process *next = cfs_queue;
    cfs_queue = next->next_ready;  
    next->next_ready = 0;
    
    return next;
}

static void task_tick_fair(struct process *p) {
    p->vruntime++;
    
    if (p->time_slice > 0) {
        p->time_slice--;
    }
}

const struct sched_class fair_sched_class = {
    .next = 0,
    .enqueue_task = enqueue_task_fair,
    .dequeue_task = dequeue_task_fair,
    .pick_next_task = pick_next_task_fair,
    .task_tick = task_tick_fair,
};
