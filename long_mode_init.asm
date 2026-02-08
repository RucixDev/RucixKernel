global start
global multiboot_info_ptr
extern long_mode_start

section .data
align 4
stack_bottom:
    resb 4096 * 4
stack_top:

global multiboot_info_ptr
global multiboot_magic

section .data
align 4
multiboot_info_ptr: dd 0
multiboot_magic: dd 0

section .trampoline
bits 32
    jmp start

section .text
bits 32
start:
    mov esp, stack_top
    mov [multiboot_info_ptr], ebx
    mov [multiboot_magic], eax

    mov dword [0xb8000], 0x2f532f53
    mov dx, 0x3f8
    mov al, 'S'
    out dx, al

    mov eax, [multiboot_magic]
    call check_multiboot
    
    mov dword [0xb8004], 0x2f4d2f4d
    mov dx, 0x3f8
    mov al, 'M'
    out dx, al

    call check_cpuid
    
    mov dword [0xb8008], 0x2f432f43
    mov dx, 0x3f8
    mov al, 'C'
    out dx, al

    call check_long_mode
    
    mov dword [0xb800c], 0x2f4c2f4c
    mov dx, 0x3f8
    mov al, 'L'
    out dx, al

    call set_up_page_tables
    
    mov dword [0xb8010], 0x2f502f50
    mov dx, 0x3f8
    mov al, 'P'
    out dx, al

    call enable_paging
    
    mov dword [0xb8014], 0x2f452f45
    mov dx, 0x3f8
    mov al, 'E'
    out dx, al

    lgdt [gdt64.pointer]

    jmp gdt64.code_segment:long_mode_start

check_multiboot:
    cmp eax, 0x36d76289
    je .success
    cmp eax, 0x2BADB002
    je .success
    cmp eax, 0x13371337
    je .success
    jmp .no_multiboot
.success:
    ret
.no_multiboot:
    mov al, "M"
    jmp error

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, "C"
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, "L"
    jmp error

set_up_page_tables:
    mov edi, p4_table
    xor eax, eax
    mov ecx, 1024
    rep stosd

    mov edi, p3_table
    xor eax, eax
    mov ecx, 1024
    rep stosd

    mov edi, p3_table_heap
    xor eax, eax
    mov ecx, 1024
    rep stosd

    mov edi, p2_table_0
    xor eax, eax
    mov ecx, 1024
    rep stosd

    mov edi, p2_table_1
    xor eax, eax
    mov ecx, 1024
    rep stosd

    mov edi, p2_table_2
    xor eax, eax
    mov ecx, 1024
    rep stosd

    mov edi, p2_table_3
    xor eax, eax
    mov ecx, 1024
    rep stosd


    mov eax, p3_table
    or eax, 0b11
    mov [p4_table], eax


    mov eax, p3_table_heap
    or eax, 0b11
    mov [p4_table + 16], eax


    mov eax, p2_table_0
    or eax, 0b11
    mov [p3_table + 0], eax
    
    mov eax, p2_table_1
    or eax, 0b11
    mov [p3_table + 8], eax

    mov eax, p2_table_2
    or eax, 0b11
    mov [p3_table + 16], eax

    mov eax, p2_table_3
    or eax, 0b11
    mov [p3_table + 24], eax

    mov ecx, 0         
.map_p2_table_0:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [p2_table_0 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_table_0

    mov ecx, 0
.map_p2_table_1:
    mov eax, 0x200000
    mul ecx
    add eax, 0x40000000
    or eax, 0b10000011
    mov [p2_table_1 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_table_1

    mov ecx, 0
.map_p2_table_2:
    mov eax, 0x200000
    mul ecx
    add eax, 0x80000000
    or eax, 0b10000011
    mov [p2_table_2 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_table_2

    mov ecx, 0
.map_p2_table_3:
    mov eax, 0x200000
    mul ecx
    add eax, 0xC0000000
    or eax, 0b10000011
    mov [p2_table_3 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_table_3

    ret

enable_paging:
    mov eax, cr0
    and ax, 0xFFFB
    or ax, 0x2
    mov cr0, eax

    mov eax, cr4
    or ax, 3 << 9
    or eax, 1 << 5
    mov cr4, eax

    mov eax, p4_table
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

error:
    mov dword [0xb8000], 0x4f524f45
    mov dword [0xb8004], 0x4f3a4f52
    mov dword [0xb8008], 0x4f204f20
    mov byte  [0xb800a], al
    
    mov dx, 0x3f8
    out dx, al
    
    hlt

section .data
align 4096
p4_table:
    times 4096 db 0
p3_table:
    times 4096 db 0
p2_table_0:
    times 4096 db 0
p2_table_1:
    times 4096 db 0
p2_table_2:
    times 4096 db 0
p2_table_3:
    times 4096 db 0
p3_table_heap:
    times 4096 db 0

section .rodata
gdt64:
    dq 0
.code_segment: equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64
