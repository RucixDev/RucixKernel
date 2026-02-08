section .multiboot_header
align 8

header_start_v1:
    dd 0x1BADB002
    dd 0x00000003
    dd -(0x1BADB002 + 0x00000003)
    

align 8
header_start_v2:
    dd 0xe85250d6
    dd 0 
    dd header_end - header_start_v2
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start_v2))
    
    align 8
    dw 5
    dw 0
    dd 24
    dd 5 
    dd 6 
    dd 0
    
    align 8
    dw 0
    dw 0
    dd 8
header_end:

extern start

section .note.Xen align=4
    dd 4
    dd 4
    dd 18
    db "Xen", 0
    dd start
