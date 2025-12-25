; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

%define KERNEL_VA            0x80000000
%define KERNEL_MEMORY_SIZE   0x00100000
%define KSTACK_TOP_VA        (KERNEL_VA + KERNEL_MEMORY_SIZE)

%define KERNEL_BASE_PA       0x00100000
%define KERNEL_DS            0x10

%define VGA_VA               0x000B8000
%define VGA_PA               0x000B8000
%define VGA_TEXT_VA          VGA_VA
%define VGA_COLS             80
%define VGA_ATTR             0x1F

%define PAGE_SIZE            0x1000
%define PAGE_SHIFT           12

%define PTE_PRESENT          0x001
%define PTE_RW               0x002
%define PTE_FLAGS            (PTE_PRESENT | PTE_RW)

%define PTE_BITS             10
%define PDE_SHIFT            (PAGE_SHIFT + PTE_BITS)
%define PDE_INDEX(va)        ((va) >> PDE_SHIFT)

%define PG_BIT               31

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

    call clear_paging_structs
    call map_kernel_pages
    call map_identity_vga
    call map_identity_trampoline

    lea eax, [paging_trampoline]
    sub eax, KERNEL_VA
    add eax, KERNEL_BASE_PA
    jmp eax


clear_paging_structs:
    xor eax, eax
    lea edi, [page_table]
    sub edi, KERNEL_VA
    add edi, KERNEL_BASE_PA
    mov ecx, (PAGE_SIZE*2)/4
    rep stosd
    ret


; eax=VA, ebx=PA, ecx=flags
map_page:
    mov edx, eax
    shr edx, PAGE_SHIFT
    and edx, 0x3FF

    lea edi, [page_table]
    sub edi, KERNEL_VA
    add edi, KERNEL_BASE_PA

    shl edx, 2
    add edi, edx

    or  ebx, ecx
    mov [edi], ebx
    ret


map_kernel_pages:
    mov esi, KERNEL_VA
    mov edi, KERNEL_BASE_PA
    mov edx, KERNEL_BASE_PA
    mov ecx, PTE_FLAGS

    mov ebp, KERNEL_MEMORY_SIZE
    shr ebp, PAGE_SHIFT

.loop:
    mov eax, esi
    mov ebx, edi
    push ecx
    call map_page
    pop ecx

    mov eax, edx
    mov ebx, edi
    push ecx
    call map_page
    pop ecx

    add esi, PAGE_SIZE
    add edx, PAGE_SIZE
    add edi, PAGE_SIZE
    dec ebp
    jnz .loop
    ret


map_identity_vga:
    mov eax, VGA_VA
    mov ebx, VGA_PA
    mov ecx, PTE_FLAGS
    call map_page
    ret


map_identity_trampoline:
    lea ebx, [paging_trampoline]
    sub ebx, KERNEL_VA
    add ebx, KERNEL_BASE_PA

    mov eax, ebx
    mov ecx, PTE_FLAGS
    call map_page
    ret


; eax=VA -> edi=PA(&PTE)
get_pte_ptr:
    mov edx, eax
    shr edx, PAGE_SHIFT
    and edx, 0x3FF

    lea edi, [page_table]
    sub edi, KERNEL_VA
    add edi, KERNEL_BASE_PA

    shl edx, 2
    add edi, edx
    ret


; eax=VA -> eax=PTE
get_pte:
    push eax
    call get_pte_ptr
    pop eax
    mov eax, [edi]
    ret


; eax=value, ecx=x, edx=y
vga_write_hex32:
    mov edi, edx
    imul edi, VGA_COLS
    add edi, ecx
    shl edi, 1
    add edi, VGA_TEXT_VA

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


; esi=str, ecx=x, edx=y
vga_write_str:
    mov edi, edx
    imul edi, VGA_COLS
    add edi, ecx
    shl edi, 1
    add edi, VGA_TEXT_VA

.str_loop:
    mov al, [esi]
    test al, al
    jz .done

    mov [edi], al
    mov byte [edi + 1], VGA_ATTR
    add esi, 1
    add edi, 2
    jmp .str_loop

.done:
    ret


align 4096
paging_trampoline:
    lea eax, [page_table]
    sub eax, KERNEL_VA
    add eax, KERNEL_BASE_PA
    or  eax, PTE_FLAGS

    lea edi, [page_directory]
    sub edi, KERNEL_VA
    add edi, KERNEL_BASE_PA

    mov [edi + 0*4], eax

    mov ebx, PDE_INDEX(KERNEL_VA)
    shl ebx, 2
    mov [edi + ebx], eax

    lea eax, [page_directory]
    sub eax, KERNEL_VA
    add eax, KERNEL_BASE_PA
    mov cr3, eax

    mov eax, cr0
    or  eax, (1 << PG_BIT)
    mov cr0, eax

  mov word [0xB8000], 0x1F40   ; '@'

    lea eax, [post_paging]
    call get_pte
    mov ecx, 0
    mov edx, 1
    call vga_write_hex32

    mov esp, KSTACK_TOP_VA
    lea eax, [post_paging]
    jmp eax


align 4096
post_paging:
    mov esp, KSTACK_TOP_VA
    call kmain


post_paging_msg:
    db "post_paging PTE:", 0
