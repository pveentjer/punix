; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

%define KSTACK_TOP_VA       0x00100000
%define KERNEL_BASE_PA      0x00100000
%define KERNEL_DS           0x10
%define PAGE_SIZE           0x1000
%define PAGE_CNT            1024
%define VGA_PAGE_IDX        0xB8

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
    or  edx, 0x03
    mov [edi], edx
    add eax, PAGE_SIZE
    add edi, 4
    loop .loop

    ret

map_identity_vga:
    lea edi, [page_table]
    mov eax, VGA_PAGE_IDX * 4
    add edi, eax
    mov dword [edi], 0xB8003
    ret

map_identity_trampoline:
    ; Calculate trampoline page index
    lea eax, [paging_trampoline]
    add eax, KERNEL_BASE_PA
    shr eax, 12

    ; Map it
    lea edi, [page_table]
    mov ebx, eax
    shl ebx, 2
    add edi, ebx

    shl eax, 12
    or  eax, 0x03
    mov [edi], eax
    ret

align 4096
paging_trampoline:
    ; Setup page directory
    lea eax, [page_table]
    or  eax, 0x03
    mov [page_directory], eax

    ; Load CR3
    lea eax, [page_directory]
    mov cr3, eax

    ; Enable paging
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    mov esp, KSTACK_TOP_VA

    mov edi, 0xB8000
    mov byte [edi], 'O'
    mov byte [edi+1], 0x0F
    mov byte [edi+2], 'K'
    mov byte [edi+3], 0x0F
    mov byte [edi+4], ' '
    mov byte [edi+5], 0x0F

    lea ebx, [post_paging]

    mov byte [edi+6], '0'
    mov byte [edi+7], 0x0F
    mov byte [edi+8], 'x'
    mov byte [edi+9], 0x0F

    mov edi, 0xB8000 + 10
    mov ecx, 8

.print_hex:
    rol ebx, 4
    mov eax, ebx
    and eax, 0x0F

    cmp eax, 9
    jle .digit
    add eax, 'A' - 10
    jmp .write_char
.digit:
    add eax, '0'

.write_char:
    mov [edi], al
    mov byte [edi+1], 0x0F
    add edi, 2
    loop .print_hex

.hang:
    hlt
    jmp .hang

align 4096
post_paging:

.hang:
    hlt
    jmp .hang

    mov edi, 0xB8000
    mov byte [edi+4], 'P'
    mov byte [edi+5], 0x0F
    mov byte [edi+6], 'O'
    mov byte [edi+7], 0x0F
    mov byte [edi+8], 'S'
    mov byte [edi+9], 0x0F
    mov byte [edi+10], 'T'
    mov byte [edi+11], 0x0F

    mov esp, KSTACK_TOP_VA
    call kmain