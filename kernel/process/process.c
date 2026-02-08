#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "console.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "spinlock.h"
#include "signal.h"
#include "drivers/irq.h"
#include "io.h"
#include "namespace.h"
#include "hrtimer.h"
#include "list.h"

struct process* current_process = 0;
struct process* process_list = 0;  
uint64_t next_pid = 1;
uint64_t global_ticks = 0;

 
 
 
struct process* sleep_queue = 0;

 
int mlfq_quantums[MLFQ_LEVELS] = { 2, 5, 10, 20 };  

extern void switch_to_task(struct process* prev, struct process* next);
extern const struct sched_class mlfq_sched_class;
extern const struct sched_class fair_sched_class;

 
void enqueue_process(struct process* proc) {
    if (proc->state != PROCESS_STATE_READY) return;

    if (proc->sched_class && proc->sched_class->enqueue_task) {
        proc->sched_class->enqueue_task(proc);
    }
}

 
void __attribute__((naked)) fork_trampoline() {
    __asm__ volatile (
        "mov current_process, %rax \n"
        "mov 16(%rax), %rsp \n"  
        "sub $176, %rsp \n"      
        "jmp isr_stub_restore \n"
    );
}

void process_init() {
     
    struct process* kernel_proc = (struct process*)kmalloc(sizeof(struct process));
    
    memset(kernel_proc, 0, sizeof(struct process));
    
    kernel_proc->pid = 0;
    kernel_proc->state = PROCESS_STATE_RUNNING;
    kernel_proc->policy = SCHED_OTHER;
    kernel_proc->sched_class = &mlfq_sched_class;  
    kernel_proc->priority = 0;
    kernel_proc->quantum = mlfq_quantums[0];
    kernel_proc->time_slice = kernel_proc->quantum;
    
    __asm__ volatile("mov %%cr3, %0" : "=r"(kernel_proc->cr3));
    
    kernel_proc->gid = 0;
    kernel_proc->sid = 0;
     
    kernel_proc->cwd = root_dentry;
    kernel_proc->cwd_mnt = root_mnt;
    
    namespaces_init();
    kernel_proc->nsproxy = &init_nsproxy;
    
    cgroup_init();
    cgroup_fork(kernel_proc);  
    
    kernel_proc->seccomp_filter = 0;

    INIT_LIST_HEAD(&kernel_proc->held_locks);

    for (int i=0; i<RLIMIT_NLIMITS; i++) {
        kernel_proc->rlimits[i].rlim_cur = RLIM_INFINITY;
        kernel_proc->rlimits[i].rlim_max = RLIM_INFINITY;
    }
    
    strcpy(kernel_proc->name, "kernel");
    
    current_process = kernel_proc;
    process_list = kernel_proc;
}

struct process* get_process_list_head() {
    return process_list;
}

struct process* get_process_by_pid(int pid) {
    struct process* curr = process_list;
    while (curr) {
        if (curr->pid == (uint64_t)pid) return curr;
        curr = curr->next;
    }
    return 0;
}

struct process* process_create(void (*entry_point)()) {
    struct process* proc = (struct process*)kmalloc(sizeof(struct process));
    if (!proc) return 0;
    
    memset(proc, 0, sizeof(struct process));
    
    proc->pid = next_pid++;
    proc->state = PROCESS_STATE_READY;
    proc->policy = SCHED_OTHER;
    proc->sched_class = &mlfq_sched_class;  
    proc->priority = 0;  
    proc->base_priority = 0;
    proc->quantum = mlfq_quantums[0];
    proc->time_slice = proc->quantum;
    
    INIT_LIST_HEAD(&proc->held_locks);

    proc->rlimits[RLIMIT_CPU].rlim_cur = -1;
    proc->rlimits[RLIMIT_CPU].rlim_max = -1;
    proc->rlimits[RLIMIT_NOFILE].rlim_cur = MAX_FILES;
    
    strcpy(proc->name, "kthread");
    
    if (current_process) {
        proc->cwd = current_process->cwd;
        proc->cwd_mnt = current_process->cwd_mnt;
         
        proc->nsproxy = current_process->nsproxy;
        proc->nsproxy->ref_count++;
    } else {
        proc->cwd = root_dentry;
        proc->cwd_mnt = root_mnt;
         
        proc->nsproxy = &init_nsproxy;
    }
    
    void* stack_phys = pmm_alloc_page(); 
    if (!stack_phys) return 0;
     
    uint64_t stack_top = (uint64_t)stack_phys + 4096;
    
    proc->kernel_stack = stack_top;
    proc->cr3 = current_process->cr3;  
    
    uint64_t* stack = (uint64_t*)stack_top;
    
    *(--stack) = (uint64_t)entry_point;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    
    proc->rsp = (uint64_t)stack;
    
    struct process* curr = process_list;
    while(curr->next) curr = curr->next;
    curr->next = proc;
    proc->prev = curr;
    
    enqueue_process(proc);
    
    return proc;
}

extern void kernel_thread_helper();

struct process* process_create_kthread(void (*entry_point)(void*), void *arg) {
    struct process* proc = (struct process*)kmalloc(sizeof(struct process));
    if (!proc) return 0;
    
    char* p = (char*)proc;
    for(uint64_t i=0; i<sizeof(struct process); i++) p[i] = 0;
    
    proc->pid = next_pid++;
    proc->state = PROCESS_STATE_READY;
    proc->policy = SCHED_OTHER;
    proc->sched_class = &mlfq_sched_class;
    proc->priority = 0;
    proc->quantum = mlfq_quantums[0];
    proc->time_slice = proc->quantum;
    
    INIT_LIST_HEAD(&proc->held_locks);

    proc->rlimits[RLIMIT_CPU].rlim_cur = -1;
    proc->rlimits[RLIMIT_CPU].rlim_max = -1;
    proc->rlimits[RLIMIT_NOFILE].rlim_cur = MAX_FILES;
    
    strcpy(proc->name, "kthread");
    
    if (current_process) {
        proc->cwd = current_process->cwd;
        proc->cwd_mnt = current_process->cwd_mnt;
        proc->nsproxy = current_process->nsproxy;
        if (proc->nsproxy) proc->nsproxy->ref_count++;
    } else {
        proc->cwd = root_dentry;
        proc->cwd_mnt = root_mnt;
        proc->nsproxy = &init_nsproxy;
    }
    
    void* stack_phys = pmm_alloc_page(); 
    if (!stack_phys) return 0;

    uint64_t stack_top = (uint64_t)stack_phys + 4096;
    proc->kernel_stack = stack_top;
    proc->cr3 = current_process ? current_process->cr3 : 0;  
    if (!proc->cr3) {
        __asm__ volatile("mov %%cr3, %0" : "=r"(proc->cr3));
    }
    
    uint64_t* stack = (uint64_t*)stack_top;
    
    *(--stack) = (uint64_t)kernel_thread_helper;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = (uint64_t)arg;  
    *(--stack) = (uint64_t)entry_point;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    
    proc->rsp = (uint64_t)stack;
    
    struct process* curr = process_list;
    while(curr->next) curr = curr->next;
    curr->next = proc;
    proc->prev = curr;
    
    enqueue_process(proc);
    
    return proc;
}

struct process* process_create_kernel_thread(void (*entry)(void), const char *name) {
    struct process *p = process_create(entry);
    if (p && name) {
        strncpy(p->name, name, sizeof(p->name) - 1);
        p->name[sizeof(p->name) - 1] = '\0';
    }
    return p;
}

void process_schedule() {
    if (!current_process) return;
    
     
    if (current_process->state == PROCESS_STATE_RUNNING) {
        current_process->state = PROCESS_STATE_READY;
        
         
        if (current_process->policy == SCHED_OTHER && current_process->time_slice <= 0) {
             if (current_process->priority < MLFQ_LEVELS - 1) {
                 current_process->priority++;
             }
             current_process->quantum = mlfq_quantums[current_process->priority];
             current_process->time_slice = current_process->quantum;
        }
        
        enqueue_process(current_process);
    }
    
     
    struct process* next = 0;
     
    if (mlfq_sched_class.pick_next_task) {
        next = mlfq_sched_class.pick_next_task(current_process);
    }
     
    if (!next && fair_sched_class.pick_next_task) {
        next = fair_sched_class.pick_next_task(current_process);
    }
    
    if (!next) {
        if (current_process->state == PROCESS_STATE_READY) {
            next = current_process;
        } else {
             __asm__ volatile("sti; hlt");
             return; 
        }
    }
    
    if (next && next != current_process) {
        struct process* prev = current_process;
        current_process = next;
        
        current_process->state = PROCESS_STATE_RUNNING;
        
        tss_set_stack(next->kernel_stack);

        switch_to_task(prev, current_process);
    } else if (next) {
         
        current_process->state = PROCESS_STATE_RUNNING;
    }
}

irqreturn_t scheduler_tick(int irq, void *dev_id) {
    (void)irq;
    (void)dev_id;
    
    hrtimer_run_queues();
    
    global_ticks++;
    if (!current_process) return IRQ_HANDLED;
    
    current_process->cpu_time++;
    current_process->time_slice--;
    
    if (current_process->policy == SCHED_CFS) {
         
        current_process->vruntime++;
    }
    
    if (check_rlimit(RLIMIT_CPU, current_process->cpu_time) != 0) {
         
        sys_kill(current_process->pid, 24);  
    }
    
    struct process* s = sleep_queue;
    struct process* prev = 0;
    
    while (s) {
        if ((uint64_t)s->waiting_on <= global_ticks) {
            struct process* wake = s;
            s = s->next_ready;  

            if (prev) prev->next_ready = s;
            else sleep_queue = s;
            
            wake->state = PROCESS_STATE_READY;
            wake->waiting_on = 0;
            enqueue_process(wake);
        } else {
            prev = s;
            s = s->next_ready;
        }
    }
    
    if (current_process->time_slice <= 0) {
         
        if (current_process->policy != SCHED_FIFO) {
            process_schedule();
        }
    }
    return IRQ_HANDLED;
}

void process_yield() {
    process_schedule();
}

void process_exit(int code) {
     
    for(int i=0; i<MAX_FILES; i++) {
        if (current_process->fd_table[i]) {
            vfs_close(i);
        }
    }

    current_process->exit_code = code;
    
    struct process* kernel_proc = get_process_by_pid(0);
    if (kernel_proc && current_process->cr3 != kernel_proc->cr3) {
        vmm_free_user_space();
    }

    kprint_str("Process Exiting PID: ");
    kprint_dec(current_process->pid);
    kprint_str(" Code: ");
    kprint_dec(code);
    kprint_newline();
    
    struct process* init_proc = get_process_by_pid(0);  
    if (get_process_by_pid(1)) init_proc = get_process_by_pid(1);

    cgroup_exit(current_process);
    
    seccomp_exit(current_process);

    struct process* p = process_list;
    while(p) {
        if (p->parent == current_process) {
            p->parent = init_proc;
        }
        p = p->next;
    }

    struct process* parent = current_process->parent;
    if (parent && parent->state == PROCESS_STATE_BLOCKED) {
        parent->state = PROCESS_STATE_READY;
        enqueue_process(parent);
    }
    
    current_process->state = PROCESS_STATE_TERMINATED;
    
    process_schedule();
    
    while(1);
}

int process_fork() {
    struct process* child = (struct process*)kmalloc(sizeof(struct process));
    if (!child) return -1;
    
    memcpy(child, current_process, sizeof(struct process));
    
    child->pid = next_pid++;
    child->parent = current_process;
    child->state = PROCESS_STATE_READY;
    child->next = 0;
    child->next_ready = 0;
     
    child->cpu_time = 0;
    child->time_slice = child->quantum;
    
    void* stack_phys = pmm_alloc_page();
    if (!stack_phys) {
         
        return -1;
    }
    child->kernel_stack = (uint64_t)stack_phys + 4096;
    
    struct interrupt_frame* parent_frame = (struct interrupt_frame*)(current_process->kernel_stack - sizeof(struct interrupt_frame));
    struct interrupt_frame* child_frame = (struct interrupt_frame*)(child->kernel_stack - sizeof(struct interrupt_frame));
    
    char* src = (char*)parent_frame;
    char* dst = (char*)child_frame;
    for(uint64_t i=0; i<sizeof(struct interrupt_frame); i++) dst[i] = src[i];
    
    child_frame->rax = 0;
    
    uint64_t* stack = (uint64_t*)child_frame;
    
    *(--stack) = (uint64_t)fork_trampoline;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    *(--stack) = 0;  
    
    child->rsp = (uint64_t)stack;
    
    struct process* curr = process_list;
    while(curr->next) curr = curr->next;
    curr->next = child;
    
    enqueue_process(child);
    
    return child->pid;
}

int process_wait(int pid, int* status) {
    while(1) {
        struct process* p = get_process_by_pid(pid);
        if (!p) return -1;
        if (p->state == PROCESS_STATE_TERMINATED) {
            if (status) *status = p->exit_code;
            return pid;
        }
        process_sleep(1);
    }
}

void process_sleep(int ticks) {
    current_process->state = PROCESS_STATE_SLEEPING;
    current_process->waiting_on = (void*)(global_ticks + ticks);

    struct process* s = sleep_queue;
    if (!s) {
        sleep_queue = current_process;
    } else {
        while(s->next_ready) s = s->next_ready;
        s->next_ready = current_process;
    }
    current_process->next_ready = 0;
    
    process_schedule();
}

int process_set_priority(int pid, int priority) {
    struct process* p = get_process_by_pid(pid);
    if (!p) return -1;
    p->priority = priority;
    return 0;
}

int process_get_priority(int pid) {
    struct process* p = get_process_by_pid(pid);
    if (!p) return -1;
    return p->priority;
}

int check_rlimit(int resource, uint64_t amount) {
    if (!current_process) return 0;
    if (resource < 0 || resource >= RLIMIT_NLIMITS) return 0;
    
    struct rlimit *lim = &current_process->rlimits[resource];
    if (lim->rlim_cur == RLIM_INFINITY) return 0;
    
    if (amount > lim->rlim_cur) return 1;
    return 0;
}
