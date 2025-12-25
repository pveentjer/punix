; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

extern __kernel_va_start
extern __kernel_pa_start
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

     ; row 0
     call get_pt_pa
     mov ecx, 0
     mov edx, 0
     call vga_write_hex32

     ; row 1
     call get_pd_pa
     mov ecx, 0
     mov edx, 1
     call vga_write_hex32

     call clear_paging_structs
     call map_identity_kernel_hdr
     call map_identity_premain
     call map_kernel_high
     call map_identity_vga

     ; row 2
     call get_pd_pa
     mov edi, eax
     mov eax, [edi]
     mov ecx, 0
     mov edx, 2
     call vga_write_hex32

     ; row 3
     call get_pd_pa
     mov edi, eax
     mov ebx, PDE_INDEX(KERNEL_VA)
     shl ebx, 2
     mov eax, [edi + ebx]
     mov ecx, 0
     mov edx, 3
     call vga_write_hex32

     ; row 4
     call get_pt_pa
     or  eax, PTE_FLAGS
     mov ecx, 0
     mov edx, 4
     call vga_write_hex32

     call get_pd_pa
     mov edi, eax

     call get_pt_pa
     or  eax, PTE_FLAGS
     mov [edi], eax

     mov ebx, PDE_INDEX(KERNEL_VA)
     shl ebx, 2
     mov [edi + ebx], eax

     ; row 5
     mov eax, [edi]
     mov ecx, 0
     mov edx, 5
     call vga_write_hex32

     call get_pd_pa
     mov cr3, eax

     ; row 6
     mov eax, cr0
     mov ecx, 0
     mov edx, 6
     call vga_write_hex32

     ; row 7
     mov eax, cr3
     mov ecx, 0
     mov edx, 7
     call vga_write_hex32

     ; row 8 — kmain before paging
     mov eax, kmain
     mov ecx, 5
     mov edx, 8
     call vga_write_hex32

     ; row 9 — Check PT[0x101] (premain mapping)
     call get_pt_pa
     mov edi, eax
     mov ebx, 0x101 * 4
     mov eax, [edi + ebx]
     mov ecx, 0
     mov edx, 9
     call vga_write_hex32

     ; row 10 — Check PT[0xB8] (VGA mapping)
     call get_pt_pa
     mov edi, eax
     mov ebx, 0xB8 * 4
     mov eax, [edi + ebx]
     mov ecx, 0
     mov edx, 10
     call vga_write_hex32

     mov eax, cr0
     or  eax, (1 << PG_BIT)
     mov cr0, eax

     mov word [VGA_VA], 0x1F40

     mov esp, KSTACK_TOP_VA
     jmp .pg_on

.pg_on:
     mov eax, kmain
     mov ecx, 0
     mov edx, 15
     call vga_write_hex32

     mov word [VGA_VA], 0x1F40
     jmp kmain

; --------------------------------------------------

va_to_pa:
     push ebx
     mov ebx, __kernel_va_start
     sub eax, ebx
     mov ebx, __kernel_pa_start
     add eax, ebx
     pop ebx
     ret

get_pt_pa:
     mov eax, __kernel_page_table_va
     call va_to_pa
     ret

get_pd_pa:
     mov eax, __kernel_page_directory_va
     call va_to_pa
     ret

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
     mov edi, __kernel_pa_start     ; FIX: match linker model (was KERNEL_LOAD_PA)
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
