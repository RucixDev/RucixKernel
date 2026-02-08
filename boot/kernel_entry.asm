section .text.entry
global start
start:
[bits 32]
[extern kernel_main]
call kernel_main

cli
.hang:
    hlt
    jmp .hang
