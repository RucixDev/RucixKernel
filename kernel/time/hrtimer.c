#include "hrtimer.h"
#include "spinlock.h"
#include "console.h"
#include "io.h"

 
static struct list_head hrtimer_list;
static spinlock_t hrtimer_lock;

#include "drivers/pit.h"

 
static uint64_t tsc_frequency = 0;  
static uint64_t initial_tsc;

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

 
static void calibrate_tsc_precise(void) {
    outb(0x43, 0x30);
    outb(0x40, 0xFF);
    outb(0x40, 0xFF);
    
    uint16_t start_pit = pit_read_count();
    uint64_t start_tsc = rdtsc();
    
    while (1) {
        uint16_t current = pit_read_count();
        if ((start_pit - current) >= 10000) break;
    }
    
    uint64_t end_tsc = rdtsc();
    uint16_t end_pit = pit_read_count();
    
    uint32_t pit_ticks = start_pit - end_pit;
    uint64_t tsc_ticks = end_tsc - start_tsc;
    
    tsc_frequency = (tsc_ticks * 1193182) / pit_ticks;
    
    kprint_str("TSC Frequency: ");
    kprint_dec(tsc_frequency / 1000000);
    kprint_str(" MHz\n");
    
    pit_init(1000); 
}

void hrtimer_init_system(void) {
    INIT_LIST_HEAD(&hrtimer_list);
    spinlock_init(&hrtimer_lock);
    
    calibrate_tsc_precise();
    
    initial_tsc = rdtsc();
    kprint_str("HRTimer: Initialized with TSC source.\n");
}

ktime_t ktime_get_ns(void) {
    uint64_t current_tsc = rdtsc();
    uint64_t diff = current_tsc - initial_tsc;
    return (diff * 1000) / (tsc_frequency / 1000000);
}

void hrtimer_init(struct hrtimer *timer) {
    INIT_LIST_HEAD(&timer->node);
    timer->function = NULL;
}

void hrtimer_start(struct hrtimer *timer, ktime_t expires) {
    spinlock_acquire(&hrtimer_lock);
    
    if (timer->node.next != 0 && timer->node.next != &timer->node && timer->node.next != NULL) {
         if (timer->node.prev) list_del(&timer->node);
    }
    
    timer->expires = expires;

    struct hrtimer *entry;
    int inserted = 0;

    struct list_head *curr = hrtimer_list.next;
    while (curr != &hrtimer_list) {
         
        entry = (struct hrtimer *)curr;
        if (entry->expires > expires) {
             
            list_add_tail(&timer->node, curr);
            inserted = 1;
            break;
        }
        curr = curr->next;
    }

    if (!inserted) {
        list_add_tail(&timer->node, &hrtimer_list);
    }
    
    spinlock_release(&hrtimer_lock);
}

void hrtimer_cancel(struct hrtimer *timer) {
    spinlock_acquire(&hrtimer_lock);
    if (timer->node.next && timer->node.prev) {
        list_del(&timer->node);
    }
    spinlock_release(&hrtimer_lock);
}

void hrtimer_run_queues(void) {
    ktime_t now = ktime_get_ns();
    
    spinlock_acquire(&hrtimer_lock);
    
    struct list_head *curr = hrtimer_list.next;
    while (curr != &hrtimer_list) {
        struct hrtimer *timer = (struct hrtimer *)curr;
        
        if (timer->expires <= now) {
            list_del(curr);  
      
            spinlock_release(&hrtimer_lock);
            
            if (timer->function) {
                if (timer->function(timer) == HRTIMER_RESTART) {}
            }
            
            spinlock_acquire(&hrtimer_lock);
            
            curr = hrtimer_list.next;
        } else {
             
            break;
        }
    }
    
    spinlock_release(&hrtimer_lock);
}
