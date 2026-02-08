#include "process.h"
#include "console.h"

 
struct process* mlfq_queues[MLFQ_LEVELS] = {0};
static struct process* mlfq_tails[MLFQ_LEVELS] = {0};  
extern int mlfq_quantums[MLFQ_LEVELS];

static void enqueue_task_mlfq(struct process *p) {
    p->state = PROCESS_STATE_READY;
    p->next_ready = 0;
    
    int level = p->priority;
    if (level >= MLFQ_LEVELS) level = MLFQ_LEVELS - 1;
    if (level < 0) level = 0;

    if (!mlfq_queues[level]) {
        mlfq_queues[level] = p;
        mlfq_tails[level] = p;
    } else {
         
        mlfq_tails[level]->next_ready = p;
        mlfq_tails[level] = p;
    }
}

static void dequeue_task_mlfq(struct process *p) {
    int level = p->priority;
    if (level >= MLFQ_LEVELS) level = MLFQ_LEVELS - 1;
    if (level < 0) level = 0;

    struct process** curr = &mlfq_queues[level];
    struct process* prev = 0;

    while (*curr) {
        if (*curr == p) {
            *curr = p->next_ready;
            
             
            if (mlfq_tails[level] == p) {
                mlfq_tails[level] = prev;
            }
            
            p->next_ready = 0;
            return;
        }
        prev = *curr;
        curr = &(*curr)->next_ready;
    }
}

static struct process *pick_next_task_mlfq(struct process *prev) {
    (void)prev;
     
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        if (mlfq_queues[i]) {
            struct process* next = mlfq_queues[i];
            mlfq_queues[i] = next->next_ready;
            
             
            if (!mlfq_queues[i]) {
                mlfq_tails[i] = 0;
            }
            
            next->next_ready = 0;
            return next;
        }
    }
    return 0;
}

static void task_tick_mlfq(struct process *p) {
    if (p->time_slice > 0) {
        p->time_slice--;
    }
    
     
     
}

const struct sched_class mlfq_sched_class = {
    .next = 0,
    .enqueue_task = enqueue_task_mlfq,
    .dequeue_task = dequeue_task_mlfq,
    .pick_next_task = pick_next_task_mlfq,
    .task_tick = task_tick_mlfq,
};
