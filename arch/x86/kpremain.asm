; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

%define KSTACK_TOP   0x00100000      ; 1 MB virtual stack
%define KERNEL_BASE  0x00100000      ; physical kernel base (1 MB)
%define KERNEL_DS    0x10

section .bss
align 4096
page_table:     resd 1024
page_directory: resd 1024

section .text
_kpremain:
    cli

    ; Setup segments
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00090000              ; Physical stack before paging

    ; Calculate trampoline page number
    lea eax, [paging_trampoline]
    add eax, KERNEL_BASE             ; Convert to physical
    shr eax, 12                      ; Get page index
    mov [trampoline_page], eax

    ; Build page table
    xor eax, eax                     ; VA counter
    lea edi, [page_table]
    mov ecx, 1024

.pt_loop:
    mov edx, eax                     ; Start with VA
    mov ebx, eax
    shr ebx, 12                      ; Get page index

    ; Identity-map first 1MB (pages 0-255) for VGA and low memory
    cmp ebx, 256
    jl .identity_map

    ; Also identity-map trampoline page
    cmp ebx, [trampoline_page]
    je .identity_map

.offset_map:
    ; Offset mapping: VA + 1MB = PA
    add edx, KERNEL_BASE
    jmp .write_pte

.identity_map:
    ; Identity mapping: VA = PA
    ; edx already contains VA

.write_pte:
    or  edx, 0x03                    ; present + RW
    mov [edi], edx
    add eax, 0x1000
    add edi, 4
    loop .pt_loop

    ; Setup page directory
    lea eax, [page_table]
    or  eax, 0x03
    mov [page_directory], eax

    lea eax, [page_directory]
    mov cr3, eax

    ; Jump to trampoline
    jmp paging_trampoline

align 4096
paging_trampoline:
    ; Enable paging
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax                     ; Paging is ON

    ; Test VGA write after paging
    mov edi, 0xB8000
    mov byte [edi], 'O'
    mov byte [edi+1], 0x0F
    mov byte [edi+2], 'K'
    mov byte [edi+3], 0x0F

    ; Hang here to verify paging + VGA works
.hang:
    hlt
    jmp .hang

align 4096
post_paging:
    ; Write to confirm we're here
    mov edi, 0xB8000
    mov byte [edi+4], 'P'
    mov byte [edi+5], 0x0F
    mov byte [edi+6], 'O'
    mov byte [edi+7], 0x0F
    mov byte [edi+8], 'S'
    mov byte [edi+9], 0x0F
    mov byte [edi+10], 'T'
    mov byte [edi+11], 0x0F

    ; Switch to virtual stack
    mov esp, KSTACK_TOP

    ; Call kernel main
    call kmain

.hang:
    hlt
    jmp .hang

section .data
trampoline_page: dd 0