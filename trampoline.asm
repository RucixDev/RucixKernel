section .trampoline
global trampoline
extern start
bits 32
trampoline:
    jmp start
    align 4
