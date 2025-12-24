; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

%define KERNEL_CS   0x08
%define KERNEL_DS   0x10
%define KSTACK_TOP  0x200000

section .text
_kpremain:
    cli

    ; NOTE: At this point we're already in 32-bit protected mode (per your setup).
    ; Use whatever segments are currently loaded just to get a stack.
    mov esp, KSTACK_TOP

    ; Load our hardcoded GDT
    lgdt [gdt_desc]

    ; Far jump to reload CS (and flush the pipeline)
    jmp KERNEL_CS:flush_cs

flush_cs:
    ; Reload data segments
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Reload SS, then set stack again (best practice: don't change ESP before SS)
    mov ss, ax
    mov esp, KSTACK_TOP

    ; Call into C kernel
    call kmain

.hang:
    hlt
    jmp .hang


; ------------------------------------------------------------
; Hardcoded GDT + GDTR
; Flat 4GiB segments, base=0, limit=0xFFFFF (with granularity=4KiB)
; ------------------------------------------------------------
section .rodata
align 8
gdt:
    dq 0x0000000000000000        ; 0x00: null descriptor

    ; 0x08: code segment: base=0, limit=4GiB, exec/read, ring0, present, 32-bit, 4KiB gran
    dq 0x00CF9A000000FFFF

    ; 0x10: data segment: base=0, limit=4GiB, read/write, ring0, present, 32-bit, 4KiB gran
    dq 0x00CF92000000FFFF

gdt_end:

gdt_desc:
    dw gdt_end - gdt - 1         ; limit
    dd gdt                       ; base
