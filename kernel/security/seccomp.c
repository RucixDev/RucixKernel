#include "seccomp.h"
#include "process.h"
#include "heap.h"
#include "console.h"
#include "string.h"

 
 
static uint32_t seccomp_run_filter(struct seccomp_data *sd, struct sock_fprog *fprog) {
    struct sock_filter *inst;
    uint32_t A = 0;
    uint32_t X = 0;
    uint32_t k;
    int pc;
    
    for (pc = 0; pc < fprog->len; pc++) {
        inst = &fprog->filter[pc];
        k = inst->k;
        
        switch (inst->code) {
            case BPF_LD+BPF_W+BPF_ABS:
                 
                if (k < sizeof(struct seccomp_data)) {
                     
                    A = *(uint32_t*)((uint8_t*)sd + k);
                } else {
                    return SECCOMP_RET_KILL;
                }
                break;
            case BPF_RET+BPF_K:
                return k;
            case BPF_RET+BPF_A:
                return A;
            case BPF_ALU+BPF_ADD+BPF_K: A += k; break;
            case BPF_ALU+BPF_SUB+BPF_K: A -= k; break;
            case BPF_ALU+BPF_MUL+BPF_K: A *= k; break;
            case BPF_ALU+BPF_DIV+BPF_K: A = (k==0) ? 0 : A/k; break;
            case BPF_ALU+BPF_AND+BPF_K: A &= k; break;
            case BPF_ALU+BPF_OR+BPF_K:  A |= k; break;
            case BPF_ALU+BPF_XOR+BPF_K: A ^= k; break;
            case BPF_ALU+BPF_LSH+BPF_K: A <<= k; break;
            case BPF_ALU+BPF_RSH+BPF_K: A >>= k; break;
            case BPF_ALU+BPF_NEG:       A = -A; break;
            case BPF_LD+BPF_IMM:        A = k; break;
            case BPF_MISC+BPF_TAX:      X = A; break;
            case BPF_MISC+BPF_TXA:      A = X; break;
            
            case BPF_JMP+BPF_JA:   pc += k; break;
            case BPF_JMP+BPF_JEQ+BPF_K: pc += (A == k) ? inst->jt : inst->jf; break;
            case BPF_JMP+BPF_JGT+BPF_K: pc += (A > k)  ? inst->jt : inst->jf; break;
            case BPF_JMP+BPF_JGE+BPF_K: pc += (A >= k) ? inst->jt : inst->jf; break;
            case BPF_JMP+BPF_JSET+BPF_K: pc += (A & k) ? inst->jt : inst->jf; break;
            
            default:
                kprint_str("BPF: Unknown instruction\n");
                return SECCOMP_RET_KILL;
        }
    }
    return SECCOMP_RET_KILL;  
}

int seccomp_check_syscall(int syscall_nr) {
    if (!current_process || !current_process->seccomp_filter) {
        return 0;  
    }
    
    struct seccomp_data sd;
    sd.nr = syscall_nr;
    sd.arch = 0xC000003E;  
    sd.instruction_pointer = 0;  
     
    struct seccomp_filter *f = current_process->seccomp_filter;
    while (f) {
        uint32_t ret = seccomp_run_filter(&sd, &f->prog);
        if ((ret & SECCOMP_RET_ACTION) != SECCOMP_RET_ALLOW) {
            kprint_str("Seccomp: Blocked syscall ");
            kprint_dec(syscall_nr);
            kprint_newline();
             
            if ((ret & SECCOMP_RET_ACTION) == SECCOMP_RET_KILL) {
                return -1;
            }
            return -1;
        }
        f = f->prev;
    }
    
    return 0;
}

int seccomp_attach_filter(struct sock_fprog *prog) {
    if (!current_process) return -1;
    
    struct seccomp_filter *filter = (struct seccomp_filter*)kmalloc(sizeof(struct seccomp_filter));
    if (!filter) return -1;
    
    filter->prog.len = prog->len;
    filter->prog.filter = (struct sock_filter*)kmalloc(prog->len * sizeof(struct sock_filter));
    memcpy(filter->prog.filter, prog->filter, prog->len * sizeof(struct sock_filter));
    
    filter->ref_count = 1;
    filter->prev = current_process->seccomp_filter;
    current_process->seccomp_filter = filter;
    
    return 0;
}

void seccomp_fork(struct process *child, struct process *parent) {
    if (!parent->seccomp_filter) {
        child->seccomp_filter = 0;
        return;
    }
    
    struct seccomp_filter *f = parent->seccomp_filter;
     
    child->seccomp_filter = f;
    
    while (f) {
        f->ref_count++;
        f = f->prev;
    }
}

void seccomp_exit(struct process *p) {
    struct seccomp_filter *f = p->seccomp_filter;
    while (f) {
        struct seccomp_filter *prev = f->prev;
        f->ref_count--;
        if (f->ref_count <= 0) {
            kfree(f->prog.filter);
            kfree(f);
        }
        f = prev;
    }
    p->seccomp_filter = 0;
}
