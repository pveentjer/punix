; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

%define KSTACK_TOP_VA       0x00100000  ; 1MB
%define KERNEL_BASE_PA      0x00100000  ; 1MB
%define KERNEL_DS           0x10
%define KERNEL_VA           0x00000000  ; the virtual address the kernel starts

%define VGA_PA              0x000B8000

%define PAGE_SIZE           0x1000  ; 4 KB
%define PAGE_CNT            1024
%define PAGE_SHIFT          12      ; Needed to convert a PA to a page index or vice versa

; Page table / directory flags
%define PTE_PRESENT         0x001
%define PTE_RW              0x002
%define PTE_FLAGS           (PTE_PRESENT | PTE_RW)

; In CR0 register, the 31 bit determines if paging is enabled
%define PG_BIT 31

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
    mov esp, 0x00090000 ; Set a temporary stack in lower memory

    ; zero page_table + page_directory
    xor eax, eax
    lea edi, [page_table]
    mov ecx, (PAGE_SIZE*2)/4
    rep stosd

    call map_offset_kernel
    call map_identity_vga
    call map_identity_trampoline

    jmp paging_trampoline


; pseudo code:

; for (int i = 0; i < PAGE_CNT; i++)
; {
;       uint32_t phys = KERNEL_BASE_PA + (i << PAGE_SHIFT);
;       uint32_t pte  = phys | PTE_FLAGS;
;       page_table[i] = pte;
; }
map_offset_kernel:
    xor eax, eax                            ; eax = page offset
    mov esi, eax                            ; esi = kernel VA (currently same as offset)
    lea edi, [page_table]
    mov ecx, PAGE_CNT

.loop:
    mov edx, eax                            ; edx = page offset
    add edx, KERNEL_BASE_PA                 ; edx = physical address of page
    or  edx, PTE_FLAGS                      ; mark page present + writable
    mov [edi], edx                          ; write PTE

    add eax, PAGE_SIZE                      ; next offset
    add esi, PAGE_SIZE                      ; keep VA in sync
    add edi, 4

    loop .loop                            ; loop untill ecx=0

map_identity_vga:
    lea edi, [page_table]

    mov eax, VGA_PA
    shr eax, PAGE_SHIFT                     ; eax = vga page index

    shl eax, 2                              ; *4 for PTE offset
    add edi, eax

    mov dword [edi], VGA_PA | PTE_FLAGS     ; write PTE
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
    or  eax, (1 << PG_BIT)
    mov cr0, eax

    mov esp, KSTACK_TOP_VA

    lea eax, [post_paging]
    jmp eax

align 4096
post_paging:
    mov esp, KSTACK_TOP_VA
    call kmain
