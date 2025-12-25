; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

%define KSTACK_TOP_VA       0x00100000  ; 1MB
%define KERNEL_BASE_PA      0x00100000  ; 1MB
%define KERNEL_DS           0x10
%define KERNEL_VA           0x00000000  ; change to e.g. 0x80000000 later

%define VGA_PA              0x000B8000

%define PAGE_SIZE           0x1000
%define PAGE_CNT            1024
%define PAGE_SHIFT          12

%define PTE_PRESENT         0x001
%define PTE_RW              0x002
%define PTE_FLAGS           (PTE_PRESENT | PTE_RW)

%define PTE_BITS            10
%define PDE_SHIFT           (PAGE_SHIFT + PTE_BITS)   ; 22
%define PDE_INDEX(va)       ((va) >> PDE_SHIFT)

%define PG_BIT              31

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

    ; zero page_table + page_directory
    xor eax, eax
    lea edi, [page_table]
    mov ecx, (PAGE_SIZE*2)/4
    rep stosd

    call map_offset_kernel
    call map_identity_vga
    call map_identity_trampoline

    ; jump to trampoline using PHYSICAL address (paging OFF)
    lea eax, [paging_trampoline]
    sub eax, KERNEL_VA
    add eax, KERNEL_BASE_PA
    jmp eax


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

    mov eax, VGA_PA
    shr eax, PAGE_SHIFT
    shl eax, 2
    add edi, eax

    mov dword [edi], VGA_PA | PTE_FLAGS
    ret


map_identity_trampoline:
    ; VA -> PA of paging_trampoline
    lea eax, [paging_trampoline]
    sub eax, KERNEL_VA
    add eax, KERNEL_BASE_PA
    shr eax, PAGE_SHIFT

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
    ; page_directory[PDE_INDEX(KERNEL_VA)] = page_table
    lea eax, [page_table]
    or  eax, PTE_FLAGS

    lea edi, [page_directory]
    mov ebx, PDE_INDEX(KERNEL_VA)
    shl ebx, 2
    mov [edi + ebx], eax

    ; load CR3 with PHYSICAL address
    lea eax, [page_directory]
    sub eax, KERNEL_VA
    add eax, KERNEL_BASE_PA
    mov cr3, eax

    ; enable paging
    mov eax, cr0
    or  eax, (1 << PG_BIT)
    mov cr0, eax

    mov esp, KSTACK_TOP_VA

    lea eax, [post_paging]
    jmp eax


align 4096
post_paging:
    mov esp, KSTACK_TOP_VA
    call kmain
