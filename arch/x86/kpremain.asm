; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

extern __kernel_va_start
extern __kernel_pa_start
extern __kernel_low_page_table_va
extern __kernel_high_page_table_va
extern __kernel_page_directory_va

%define PAGE_SIZE            4096
%define PAGE_SHIFT           12

%define KERNEL_VA            2148532224          ; 0x80100000
%define KERNEL_MEMORY_SIZE   1048576             ; 1MB window
%define KSTACK_TOP_VA        (KERNEL_VA + KERNEL_MEMORY_SIZE - PAGE_SIZE)

%define KERNEL_LOAD_PA       1048576             ; 0x00100000
%define PREMAIN_PA           KERNEL_LOAD_PA
%define KERNEL_HDR_PA        (KERNEL_LOAD_PA + PAGE_SIZE)

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

section .text.premain

_kpremain:
    cli

    ; Flat segments
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Temporary stack in identity-mapped low memory
    mov esp, 589824

    ; Zero PD + both PTs
    call clear_paging_structs

    ; Fill low PT: identity map premain/header/VGA
    call map_identity_premain
    call map_identity_kernel_hdr
    call map_identity_vga

    ; Fill high PT: map 1MB window at KERNEL_VA -> __kernel_pa_start
    call map_kernel_high

    ; Install PDE[0] = low PT (identity mappings)
    call get_low_pt_pa
    mov ebx, eax
    mov eax, 0
    call install_pt_at_pde_pa

    ; Install PDE[KERNEL_VA>>22] = high PT (high kernel window)
    call get_high_pt_pa
    mov ebx, eax
    mov eax, PDE_INDEX(KERNEL_VA)
    call install_pt_at_pde_pa

    ; Load page directory base
    call get_pd_pa
    mov cr3, eax

    ; Enable paging
    mov eax, cr0
    or  eax, (1 << PG_BIT)
    mov cr0, eax

    ; If you don't see '@', paging died before/at this store
    ;mov word [VGA_VA], 0x1F40          ; '@'

    ; Switch to high virtual stack
    mov esp, KSTACK_TOP_VA
    jmp .pg_on

.pg_on:
    jmp kmain

; --------------------------------------------------
; VA->PA translation for symbols that live in the high kernel window.
; Input : eax = VA
; Output: eax = PA
; Clobbers: ebx
va_to_pa:
    push ebx
    mov ebx, __kernel_va_start
    sub eax, ebx
    mov ebx, __kernel_pa_start
    add eax, ebx
    pop ebx
    ret

; Page table / directory physical addresses (from linker VAs)
get_low_pt_pa:
    mov eax, __kernel_low_page_table_va
    call va_to_pa
    ret

get_high_pt_pa:
    mov eax, __kernel_high_page_table_va
    call va_to_pa
    ret

get_pd_pa:
    mov eax, __kernel_page_directory_va
    call va_to_pa
    ret

; --------------------------------------------------
; Clear paging structures: low PT, high PT, PD.
; Clobbers: eax, ecx, edi
clear_paging_structs:
    xor eax, eax

    call get_low_pt_pa
    mov edi, eax
    mov ecx, PAGE_SIZE / 4
    rep stosd

    call get_high_pt_pa
    mov edi, eax
    mov ecx, PAGE_SIZE / 4
    rep stosd

    call get_pd_pa
    mov edi, eax
    mov ecx, PAGE_SIZE / 4
    rep stosd
    ret

; --------------------------------------------------
; Install a PT into PD at PDE index.
; Input: eax = pde_index, ebx = pt_pa
install_pt_at_pde_pa:
    push ecx
    push edi

    mov ecx, eax                 ; pde index

    call get_pd_pa
    mov edi, eax                 ; PD base (PA)

    mov eax, ebx                 ; PT physical address
    or  eax, PTE_FLAGS           ; PDE flags

    shl ecx, 2                   ; index * 4
    mov [edi + ecx], eax

    pop edi
    pop ecx
    ret

; --------------------------------------------------
; Write a PTE into a given PT.
; Input: eax = VA, ebx = PA, ecx = flags, edi = pt_base_pa
map_page_pt:
    mov edx, eax
    shr edx, PAGE_SHIFT
    and edx, 0x3FF
    shl edx, 2

    mov eax, ebx
    or  eax, ecx
    mov [edi + edx], eax
    ret

; Identity mappings all go through low PT
map_identity_premain:
    call get_low_pt_pa
    mov edi, eax
    mov eax, PREMAIN_PA
    mov ebx, PREMAIN_PA
    mov ecx, PTE_FLAGS
    call map_page_pt
    ret

map_identity_kernel_hdr:
    call get_low_pt_pa
    mov edi, eax
    mov eax, KERNEL_HDR_PA
    mov ebx, KERNEL_HDR_PA
    mov ecx, PTE_FLAGS
    call map_page_pt
    ret

map_identity_vga:
    call get_low_pt_pa
    mov edi, eax
    mov eax, VGA_VA
    mov ebx, VGA_PA
    mov ecx, PTE_FLAGS
    call map_page_pt
    ret

; --------------------------------------------------
; Map KERNEL_MEMORY_SIZE bytes at:
;   VA: KERNEL_VA .. KERNEL_VA+KERNEL_MEMORY_SIZE
;   PA: __kernel_pa_start .. +KERNEL_MEMORY_SIZE
; Uses the high PT.
map_kernel_high:
    call get_high_pt_pa
    mov edi, eax                 ; high PT base (PA)

    mov esi, KERNEL_VA           ; VA cursor
    mov ebx, __kernel_pa_start   ; PA cursor
    mov ecx, PTE_FLAGS

    mov ebp, KERNEL_MEMORY_SIZE
    shr ebp, PAGE_SHIFT          ; number of pages

.loop:
    mov eax, esi
    call map_page_pt

    add esi, PAGE_SIZE
    add ebx, PAGE_SIZE
    dec ebp
    jnz .loop
    ret

; --------------------------------------------------
; Debug helper (kept, unused here)
; eax=value, ecx=col, edx=row
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