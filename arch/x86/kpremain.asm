; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

%define KSTACK_TOP_VA       0x00100000
%define KERNEL_BASE_PA      0x00100000
%define KERNEL_DS           0x10

%define PAGE_SIZE           0x1000
%define PAGE_CNT            1024
%define PAGE_SHIFT          12

%define VGA_PAGE_IDX        0xB8

; Page table / directory flags
%define PTE_PRESENT         0x001
%define PTE_RW              0x002
%define PTE_FLAGS           (PTE_PRESENT | PTE_RW)

section .bss
align 4096
page_table:     resd 1024
page_directory: resd 1024

section .text

_kpremain:
    cli

    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00090000

    call map_offset_kernel
    call map_identity_vga
    call map_identity_trampoline

    jmp paging_trampoline

map_offset_kernel:
    xor eax, eax
    lea edi, [page_table]
    mov ecx, PAGE_CNT

.loop:
    mov edx, eax
    add edx, KERNEL_BASE_PA
    or  edx, PTE_FLAGS
    mov [edi], edx
    add eax, PAGE_SIZE
    add edi, 4
    loop .loop
    ret

map_identity_vga:
    lea edi, [page_table]
    mov eax, VGA_PAGE_IDX * 4
    add edi, eax
    mov dword [edi], 0xB8000 | PTE_FLAGS
    ret

map_identity_trampoline:
    ; Calculate trampoline page index
    lea eax, [paging_trampoline]
    add eax, KERNEL_BASE_PA
    shr eax, PAGE_SHIFT

    ; Map it
    lea edi, [page_table]
    mov ebx, eax
    shl ebx, 2
    add edi, ebx

    shl eax, PAGE_SHIFT
    or  eax, PTE_FLAGS
    mov [edi], eax
    ret

align 4096
paging_trampoline:
    ; Setup page directory
    lea eax, [page_table]
    or  eax, PTE_FLAGS
    mov [page_directory], eax

    ; Load CR3
    lea eax, [page_directory]
    mov cr3, eax

    ; Enable paging
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    mov esp, KSTACK_TOP_VA

    lea eax, [post_paging]
    jmp eax

align 4096
post_paging:
    mov esp, KSTACK_TOP_VA
    call kmain
