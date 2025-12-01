; loader.asm - 16-bit loader, enters 32-bit mode and jumps to kernel
bits 16
org 0x7E00

start:
    cli
    mov ax, 0x0000
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    ; load kernel (2 sectors) to 0x1000:0x0000 = 0x10000 physical
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x0000
    mov ah, 0x02
    mov al, 4           ; load 4 sectors (kernel size ~2KB)
    mov ch, 0
    mov cl, 4
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; setup GDT
    lgdt [gdt_descriptor]

    ; set PE bit (bit 0) in CR0
    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    ; far jump to flush pipeline and enter 32-bit mode
    jmp CODE_SEG:init_pm

; ------------------------------------------------------------
; 32-bit protected mode
; ------------------------------------------------------------
bits 32
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000         ; simple stack

    call 0x10000             ; jump to kernel (physical addr of kernel start)

.hang:
    cli
    hlt
    jmp .hang

disk_error:
    cli
    hlt
    jmp disk_error

; ---------------- GDT ----------------
gdt_start:
    dq 0x0000000000000000       ; null descriptor
    dq 0x00CF9A000000FFFF       ; code segment
    dq 0x00CF92000000FFFF       ; data segment
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10

boot_drive db 0
times 1024-($-$$) db 0

