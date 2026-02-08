global switch_to_task
global kernel_thread_helper
extern process_exit

switch_to_task:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    
    mov [rdi + 8], rsp
    
    mov rsp, [rsi + 8]
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    
    ret



kernel_thread_helper:
    mov rdi, r12
    call r13
    mov rdi, rax
    call process_exit
