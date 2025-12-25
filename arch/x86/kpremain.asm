; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain


%define PAGE_SIZE            4096
%define PAGE_SHIFT           12

%define KERNEL_VA            2148532224        ; 0x80100000
%define KERNEL_MEMORY_SIZE   1048576           ; 1 MiB
%define KSTACK_TOP_VA        (KERNEL_VA + KERNEL_MEMORY_SIZE)

%define KERNEL_LOAD_PA       1048576           ; 1 MiB
%define KERNEL_HDR_PA        KERNEL_LOAD_PA
%define PREMAIN_PA           (KERNEL_LOAD_PA + PAGE_SIZE)

%define KERNEL_DS            0x10

%define VGA_PA               0xB8000
%define VGA_VA               VGA_PA
%define VGA_COLS             80
%define VGA_ATTR             0x1F


%define PTE_PRESENT          1
%define PTE_RW               2
%define PTE_FLAGS            (PTE_PRESENT | PTE_RW)

%define PTE_BITS             10
%define PDE_SHIFT            (PAGE_SHIFT + PTE_BITS)
%define PDE_INDEX(va)        ((va) >> PDE_SHIFT)

%define PG_BIT               31

section .bss
align PAGE_SIZE
page_table:     resd 1024
page_directory: resd 1024

section .text.premain

_kpremain:

    mov word [753664], 0x1F40

    cli

    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 589824            ; 0x90000


    call clear_paging_structs

    call map_identity_kernel_hdr
    call map_identity_premain
    call map_identity_vga
    call map_kernel_high

    ; debug hooks (optional)
    ; mov eax, KERNEL_HDR_PA
    ; mov ecx, 0
    ; mov edx, 0
    ; call vga_write_hex32

    call enable_paging

    mov esp, KSTACK_TOP_VA
    jmp kmain

; --------------------------------------------------

clear_paging_structs:
    xor eax, eax
    lea edi, [page_table]
    mov ecx, (PAGE_SIZE*2)/4
    rep stosd
    ret

; eax = VA, ebx = PA, ecx = flags
map_page:
    mov edx, eax
    shr edx, PAGE_SHIFT
    and edx, 0x3FF

    lea edi, [page_table]
    shl edx, 2
    add edi, edx

    or  ebx, ecx
    mov [edi], ebx
    ret

; --------------------------------------------------
; Identity mappings
; --------------------------------------------------

map_identity_kernel_hdr:
    mov eax, KERNEL_HDR_PA
    mov ebx, KERNEL_HDR_PA
    mov ecx, PTE_FLAGS
    call map_page
    ret

map_identity_premain:
    mov eax, PREMAIN_PA
    mov ebx, PREMAIN_PA
    mov ecx, PTE_FLAGS
    call map_page
    ret

map_identity_vga:
    mov eax, VGA_VA
    mov ebx, VGA_PA
    mov ecx, PTE_FLAGS
    call map_page
    ret

; --------------------------------------------------
; High kernel mapping (VA -> PA)
; --------------------------------------------------

map_kernel_high:
    mov esi, KERNEL_VA
    mov edi, KERNEL_LOAD_PA
    mov ecx, PTE_FLAGS

    mov ebp, KERNEL_MEMORY_SIZE
    shr ebp, PAGE_SHIFT

.loop:
    mov eax, esi
    mov ebx, edi
    push ecx
    call map_page
    pop ecx

    add esi, PAGE_SIZE
    add edi, PAGE_SIZE
    dec ebp
    jnz .loop
    ret

; --------------------------------------------------

enable_paging:
    lea eax, [page_table]
    or  eax, PTE_FLAGS

    lea edi, [page_directory]
    mov [edi + 0*4], eax

    mov ebx, PDE_INDEX(KERNEL_VA)
    shl ebx, 2
    mov [edi + ebx], eax

    lea eax, [page_directory]
    mov cr3, eax

    mov eax, cr0
    or  eax, (1 << PG_BIT)
    mov cr0, eax
    ret

; --------------------------------------------------
; Debug VGA
; --------------------------------------------------

; eax=value, ecx=x, edx=y
vga_write_hex32:
    mov edi, edx
    imul edi, VGA_COLS
    add edi, ecx
    shl edi, 1
    add edi, VGA_VA

    mov esi, 8
.hex_loop:
    mov ebx, eax
    shr ebx, 28
    and ebx, 0xF

    cmp bl, 9
    jbe .digit
    add bl, ('A' - 10)
    jmp .emit
.digit:
    add bl, '0'

.emit:
    mov byte [edi], bl
    mov byte [edi + 1], VGA_ATTR
    add edi, 2

    shl eax, 4
    dec esi
    jnz .hex_loop
    ret
