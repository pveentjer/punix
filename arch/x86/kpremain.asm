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

%define KERNEL_VA            2148532224
%define KERNEL_MEMORY_SIZE   1048576
%define KSTACK_TOP_VA        (KERNEL_VA + KERNEL_MEMORY_SIZE - PAGE_SIZE)

%define KERNEL_LOAD_PA       1048576
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

section .text.premain

_kpremain:
     cli

     mov ax, KERNEL_DS
     mov ds, ax
     mov es, ax
     mov fs, ax
     mov gs, ax
     mov ss, ax
     mov esp, 589824

     call clear_paging_structs
     call map_identity_kernel_hdr
     call map_identity_premain
     call map_kernel_high
     call map_identity_vga

     ; DEBUG: Print kmain address BEFORE paging
     mov eax, kmain
     mov ecx, 0
     mov edx, 10
     call vga_write_hex32

    ; Install low page table for identity mappings (first 4MB)
    call get_low_pt_pa
    mov ebx, eax          ; ebx = PT PA
    mov eax, 0            ; eax = PDE index
    call install_pt_at_pde_pa

    ; Install high page table for high kernel (0x80100000+)
    call get_high_pt_pa
    mov ebx, eax          ; ebx = PT PA
    mov eax, PDE_INDEX(KERNEL_VA)  ; eax = PDE index
    call install_pt_at_pde_pa

    call get_pd_pa
     mov cr3, eax

     mov eax, cr0
     or  eax, (1 << PG_BIT)

mov word [VGA_VA], 0x1F40


     mov cr0, eax



     mov esp, KSTACK_TOP_VA
     jmp .pg_on

.pg_on:
     jmp kmain

; --------------------------------------------------
; Convert virtual address to physical address
; Input:  eax = virtual address
; Output: eax = physical address
; Clobbers: ebx
va_to_pa:
     push ebx
     mov ebx, __kernel_va_start
     sub eax, ebx
     mov ebx, __kernel_pa_start
     add eax, ebx
     pop ebx
     ret

; --------------------------------------------------
; Get physical address of low page table (for identity mappings)
; Input:  none
; Output: eax = physical address of low page table
; Clobbers: ebx (via va_to_pa)
get_low_pt_pa:
     mov eax, __kernel_low_page_table_va
     call va_to_pa
     ret

; --------------------------------------------------
; Get physical address of high page table (for high kernel mappings)
; Input:  none
; Output: eax = physical address of high page table
; Clobbers: ebx (via va_to_pa)
get_high_pt_pa:
     mov eax, __kernel_high_page_table_va
     call va_to_pa
     ret

; --------------------------------------------------
; Get physical address of page directory
; Input:  none
; Output: eax = physical address of page directory
; Clobbers: ebx (via va_to_pa)
get_pd_pa:
     mov eax, __kernel_page_directory_va
     call va_to_pa
     ret

; --------------------------------------------------
; Clear all paging structures (both page tables and page directory)
; Input:  none
; Output: none
; Clobbers: eax, ecx, edi, ebx (via get_*_pa calls)
clear_paging_structs:
     xor eax, eax

     ; Clear low page table
     call get_low_pt_pa
     mov edi, eax
     mov ecx, PAGE_SIZE / 4
     rep stosd

     ; Clear high page table
     call get_high_pt_pa
     mov edi, eax
     mov ecx, PAGE_SIZE / 4
     rep stosd

     ; Clear page directory
     call get_pd_pa
     mov edi, eax
     mov ecx, PAGE_SIZE / 4
     rep stosd
     ret

; --------------------------------------------------
; Install a page table into the page directory at the given index
; Input:  eax = PDE index (0-1023)
;         ebx = page table physical address
; Output: none
; Clobbers: eax, ecx, edi, ebx (via get_pd_pa)
install_pt_at_pde_pa:
     push ebx
     push ecx
     mov ecx, eax            ; save PDE index
     mov ebx, ebx            ; PT PA already in ebx

     call get_pd_pa
     mov edi, eax

     mov eax, ebx
     or  eax, PTE_FLAGS

     shl ecx, 2              ; index * 4
     mov [edi + ecx], eax
     pop ecx
     pop ebx
     ret

; --------------------------------------------------
; Map a single page using the specified page table
; Input:  eax = virtual address
;         ebx = physical address
;         ecx = PTE flags (e.g., PTE_PRESENT | PTE_RW)
;         edi = page table physical address
; Output: none
; Clobbers: edx, esi, edi
map_page_pt:
     mov edx, eax
     shr edx, PAGE_SHIFT
     and edx, 0x3FF

     shl edx, 2
     add edi, edx

     mov esi, ebx
     or  esi, ecx
     mov [edi], esi
     ret

; --------------------------------------------------
; Map kernel header page (identity mapping at 1MB)
; Input:  none
; Output: none
; Clobbers: eax, ebx, ecx, edi, edx, esi (via map_page_pt)
map_identity_kernel_hdr:
     mov eax, KERNEL_HDR_PA
     mov ebx, KERNEL_HDR_PA
     mov ecx, PTE_FLAGS
     call get_low_pt_pa
     mov edi, eax
     call map_page_pt
     ret

; --------------------------------------------------
; Map premain page (identity mapping at 1MB + 4KB)
; Input:  none
; Output: none
; Clobbers: eax, ebx, ecx, edi, edx, esi (via map_page_pt)
map_identity_premain:
     mov eax, PREMAIN_PA
     mov ebx, PREMAIN_PA
     mov ecx, PTE_FLAGS
     call get_low_pt_pa
     mov edi, eax
     call map_page_pt
     ret

; --------------------------------------------------
; Map VGA text buffer (identity mapping at 0xB8000)
; Input:  none
; Output: none
; Clobbers: eax, ebx, ecx, edi, edx, esi (via map_page_pt)
map_identity_vga:
     mov eax, VGA_VA
     mov ebx, VGA_PA
     mov ecx, PTE_FLAGS
     call get_low_pt_pa
     mov edi, eax
     call map_page_pt
     ret

; --------------------------------------------------
; Map entire kernel to high memory (starting at KERNEL_VA)
; Maps KERNEL_MEMORY_SIZE bytes from physical __kernel_pa_start
; to virtual KERNEL_VA
; Input:  none
; Output: none
; Clobbers: eax, ebx, ecx, edi, esi, ebp, edx (via map_page_pt)
map_kernel_high:
     call get_high_pt_pa
     push eax                ; save high PT PA

     mov esi, KERNEL_VA
     mov edi, __kernel_pa_start
     mov ecx, PTE_FLAGS

     mov ebp, KERNEL_MEMORY_SIZE
     shr ebp, PAGE_SHIFT

.loop:
     mov eax, esi
     mov ebx, edi
     push ecx
     pop ecx                 ; restore flags

     pop edi                 ; get high PT PA
     push edi                ; save it again
     push ecx
     call map_page_pt
     pop ecx

     add esi, PAGE_SIZE
     add edi, PAGE_SIZE
     dec ebp
     jnz .loop

     pop eax                 ; clean up stack
     ret

; --------------------------------------------------
; Write a 32-bit hex value to VGA text buffer
; Input:  eax = 32-bit value to display
;         ecx = column (0-79)
;         edx = row (0-24)
; Output: none
; Clobbers: eax, ebx, ecx, edx, edi, esi
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