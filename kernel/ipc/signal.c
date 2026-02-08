#include "signal.h"
#include "process.h"
#include "console.h"
#include "idt.h"
#include "string.h"

extern struct process* current_process;

int sys_kill(int pid, int sig) {
     
    if (pid > 0) {
        struct process* target = get_process_by_pid(pid);
        if (!target) return -1;
        
        if (sig > 0 && sig < 32) {
            target->pending_signals |= (1 << sig);
            if (target->state == PROCESS_STATE_BLOCKED) {
                target->state = PROCESS_STATE_READY;
                 
                enqueue_process(target);
            }
            return 0;
        }
        return -1;
    }
    
     
     
     
    if (pid == -1) {
        struct process* p = get_process_list_head();
         
        while(p) {
             
            if (p->pid > 1 && p != current_process) {
                if (sig > 0 && sig < 32) {
                    p->pending_signals |= (1 << sig);
                    if (p->state == PROCESS_STATE_BLOCKED) {
                        p->state = PROCESS_STATE_READY;
                        enqueue_process(p);
                    }
                     
                }
            }
            p = p->next;
        }
        return 0;
    }
    
    int pgid = (pid == 0) ? current_process->gid : -pid;
    
    struct process* p = get_process_list_head();
    int count = 0;
    
    while (p) {
        if (p->gid == (uint64_t)pgid) {
            if (sig > 0 && sig < 32) {
                p->pending_signals |= (1 << sig);
                if (p->state == PROCESS_STATE_BLOCKED) {
                    p->state = PROCESS_STATE_READY;
                    enqueue_process(p);
                }
                count++;
            }
        }
        p = p->next;
    }
    
    return (count > 0) ? 0 : -1;
}

sighandler_t sys_signal(int sig, sighandler_t handler) {
    if (sig <= 0 || sig >= 32) return (sighandler_t)-1;
    
    sighandler_t old = (sighandler_t)current_process->signal_handler[sig];
    current_process->signal_handler[sig] = (uint64_t)handler;
    return old;
}

 
struct sigframe {
    uint8_t ret_code[8];  
    struct interrupt_frame tf;
};

static void setup_frame(int sig, uint64_t handler, struct interrupt_frame *frame) {
     
    uint64_t rsp = frame->rsp;
    rsp -= sizeof(struct sigframe);
    rsp &= ~0xF;  
    
    struct sigframe *sf = (struct sigframe*)rsp;
    
     
    memcpy(&sf->tf, frame, sizeof(struct interrupt_frame));
    
     
     
    sf->ret_code[0] = 0xB8;
    sf->ret_code[1] = 119; 
    sf->ret_code[2] = 0; 
    sf->ret_code[3] = 0; 
    sf->ret_code[4] = 0;
    sf->ret_code[5] = 0xCD;
    sf->ret_code[6] = 0x80;
    
     
    frame->rsp = rsp;
    frame->rip = handler;
    frame->rdi = sig;  
    
     
    frame->rsp -= 8;
    *(uint64_t*)frame->rsp = (uint64_t)&sf->ret_code;
}

void handle_pending_signals(struct interrupt_frame* frame) {
    if (!current_process) return;
    
    uint32_t signals = current_process->pending_signals;
    if (signals == 0) return;
    
    for (int i = 1; i < 32; i++) {
        if (signals & (1 << i)) {
             
            current_process->pending_signals &= ~(1 << i);
            
            uint64_t handler = current_process->signal_handler[i];
            
            if (handler == 0) {
                 
                kprint_str("Terminated by signal: ");
                kprint_dec(i);
                kprint_newline();
                process_exit(128 + i); 
            } else if (handler == 1) {
                 
                continue;
            } else {
                 
                setup_frame(i, handler, frame);
                return;  
            }
        }
    }
}

int sys_sigreturn(struct interrupt_frame *frame) {
     
     
     
     
     
     
     
     
     
     
     
     
     
     
    
    struct sigframe *sf = (struct sigframe*)frame->rsp;
    
     
    memcpy(frame, &sf->tf, sizeof(struct interrupt_frame));
    
    return frame->rax;  
}
