#ifndef HRTIMER_H
#define HRTIMER_H

#include "types.h"
#include "list.h"
#include "spinlock.h"

typedef uint64_t ktime_t;  

struct hrtimer;

enum hrtimer_restart {
    HRTIMER_NORESTART,
    HRTIMER_RESTART,
};

typedef enum hrtimer_restart (*hrtimer_func_t)(struct hrtimer *timer);

struct hrtimer {
    struct list_head node;
    ktime_t expires;
    hrtimer_func_t function;
    void *data;
};

void hrtimer_init_system(void);
void hrtimer_init(struct hrtimer *timer);
void hrtimer_start(struct hrtimer *timer, ktime_t expires);
void hrtimer_cancel(struct hrtimer *timer);
void hrtimer_run_queues(void);  

ktime_t ktime_get_ns(void);

#endif
