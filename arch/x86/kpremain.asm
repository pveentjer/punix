; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

extern __kernel_page_table_va
extern __kernel_page_directory_va

%define PAGE_SIZE            4096
%define PAGE_SHIFT           12

%define KERNEL_VA            2148532224
%define KERNEL_MEMORY_SIZE   1048576
%define KSTACK_TOP_VA        (KERNEL_VA + KERNEL_MEMORY_SIZE)

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

    ; row 0: PT_PA
    call get_pt_pa
    mov ecx, 0
    mov edx, 0
    call vga_write_hex32

    ; row 1: PD_PA
    call get_pd_pa
    mov ecx, 0
    mov edx, 1
    call vga_write_hex32

    call clear_paging_structs
    call map_identity_kernel_hdr
    call map_identity_premain
    call map_kernel_high
 call map_identity_vga

    ; row 2: PDE[0]
    call get_pd_pa
    mov edi, eax
    mov eax, [edi]
    mov ecx, 0
    mov edx, 2
    call vga_write_hex32

    ; row 3: PDE[KERNEL]
    call get_pd_pa
    mov edi, eax
    mov ebx, PDE_INDEX(KERNEL_VA)
    shl ebx, 2
    mov eax, [edi + ebx]
    mov ecx, 0
    mov edx, 3
    call vga_write_hex32

    ; row 4: PT_PA | flags (value to be written)
    call get_pt_pa
    or  eax, PTE_FLAGS
    mov ecx, 0
    mov edx, 4
    call vga_write_hex32

    ; write PDEs
    call get_pd_pa
    mov edi, eax

    call get_pt_pa
    or  eax, PTE_FLAGS
    mov [edi], eax

    mov ebx, PDE_INDEX(KERNEL_VA)
    shl ebx, 2
    mov [edi + ebx], eax

    ; row 5: PDE[0] after write
    mov eax, [edi]
    mov ecx, 0
    mov edx, 5
    call vga_write_hex32

    call get_pd_pa
    mov cr3, eax

; Debug: print CR0
mov eax, cr0
mov ecx, 0
mov edx, 6
call vga_write_hex32

; Debug: print CR3
mov eax, cr3
mov ecx, 0
mov edx, 7
call vga_write_hex32

mov word [VGA_VA], 0x1F40

    mov eax, cr0
    or  eax, (1 << PG_BIT)
    mov cr0, eax



    jmp .pg_on

.pg_on:
mov word [VGA_VA], 0x1F40


    mov esp, KSTACK_TOP_VA
    mov word [VGA_VA], 0x1F40
    jmp kmain

; --------------------------------------------------

va_to_pa:
    sub eax, KERNEL_VA
    add eax, KERNEL_LOAD_PA
    ret

get_pt_pa:
    mov eax, __kernel_page_table_va
    jmp va_to_pa

get_pd_pa:
    mov eax, __kernel_page_directory_va
    jmp va_to_pa

clear_paging_structs:
    xor eax, eax

    call get_pt_pa
    mov edi, eax
    mov ecx, PAGE_SIZE / 4
    rep stosd

    call get_pd_pa
    mov edi, eax
    mov ecx, PAGE_SIZE / 4
    rep stosd
    ret

map_page:
    mov edx, eax
    shr edx, PAGE_SHIFT
    and edx, 0x3FF

    call get_pt_pa
    mov edi, eax

    shl edx, 2
    add edi, edx

    mov esi, ebx
    or  esi, ecx
    mov [edi], esi
    ret

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
