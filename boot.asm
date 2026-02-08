[org 0x7c00]
KERNEL_OFFSET equ 0x8000 

    xor ax, ax
    mov ss, ax
    mov sp, 0x7c00
    mov bp, sp

    mov [BOOT_DRIVE], dl 

    mov si, MSG_REAL_MODE
    call print_string

    call load_kernel     

    call switch_to_pm    

    jmp $



print_string:
    pusha
    mov ah, 0x0e
.loop:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

print_hex:
    pusha
    mov cx, 0
.loop:
    cmp cx, 4
    je .done
    mov ax, dx
    and ax, 0xf000
    shr ax, 12
    add al, 0x30
    cmp al, 0x39
    jle .step2
    add al, 7
.step2:
    mov ah, 0x0e
    int 0x10
    shl dx, 4
    inc cx
    jmp .loop
.done:
    popa
    ret

disk_load:
    pusha
    
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc .no_lba
    
    mov si, dap
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error
    
    popa
    ret

.no_lba:
    mov si, MSG_NO_LBA
    call print_string
    jmp $

disk_error:
    mov dx, ax      
    mov si, MSG_DISK_ERROR
    call print_string
    call print_hex  
    jmp $

load_kernel:
    mov si, MSG_LOAD_KERNEL
    call print_string
    

    call disk_load
    ret


dap:
    db 0x10
    db 0x00
    dw 1024
    dw KERNEL_OFFSET
    dw 0x0000
    dq 1


gdt_start:
gdt_null:
    dd 0x0
    dd 0x0
gdt_code:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x90000
    mov esp, ebp
    call BEGIN_PM

BEGIN_PM:
    mov esi, KERNEL_OFFSET
    mov edi, 0x100000
    mov ecx, 0x20000
    rep movsd

    mov eax, 0x13371337
    xor ebx, ebx
    call 0x100000  
    jmp $

BOOT_DRIVE db 0
MSG_REAL_MODE db "Started in 16-bit Real Mode", 0x0D, 0x0A, 0
MSG_DISK_ERROR db "Disk read error", 0
MSG_NO_LBA db "No LBA support", 0
MSG_LOAD_KERNEL db "Loading kernel into memory", 0x0D, 0x0A, 0

times 510-($-$$) db 0
dw 0xaa55
