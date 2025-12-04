; loader.asm - 16-bit loader, enters 32-bit mode and jumps to kernel
bits 16
org 0x7E00

%define KERNEL_LOAD_TEMP   0x8000       ; 512 KB temp load area (0x00080000)
%define KERNEL_TARGET      0x00100000   ; final 1 MiB kernel addr
%define KERNEL_SECTORS     128          ; sectors to read (128 * 512 = 64 KiB)

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    ; load kernel temporarily at 0x8000:0000 = 0x00080000 (512 KB)
    mov ax, KERNEL_LOAD_TEMP
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, KERNEL_SECTORS   ; sectors to read
    mov ch, 0
    mov cl, 4                ; kernel starts at sector 4
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    lgdt [gdt_descriptor]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax
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
    mov esp, 0x90000

    ; copy kernel from 0x00080000 -> 0x00100000
    mov esi, 0x00080000
    mov edi, KERNEL_TARGET
    mov ecx, (KERNEL_SECTORS * 512) / 4   ; sectors × 512 B ÷ 4 B/word
    rep movsd

    ; jump to kernel
    jmp KERNEL_TARGET

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
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10

boot_drive db 0
times 1024-($-$$) db 0
