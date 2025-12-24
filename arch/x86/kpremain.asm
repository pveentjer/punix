; arch/x86/kpremain.asm
BITS 32
global _kpremain
extern kmain

%define KSTACK_TOP   0x00100000      ; 1 MB virtual stack
%define KERNEL_BASE  0x00100000      ; physical kernel base (1 MB)
%define KERNEL_DS    0x10

section .bss
align 4096
page_table:     resd 1024
page_directory: resd 1024

section .text
_kpremain:
    cli

    ; ------------------------------------------------------------
    ; Setup segments first
    ; ------------------------------------------------------------
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00090000              ; Physical stack before paging

    ; ------------------------------------------------------------
    ; Step 1 & 2: Build page table
    ; First, calculate trampoline page number
    ; ------------------------------------------------------------
    lea eax, [paging_trampoline]
    add eax, KERNEL_BASE             ; Convert link address to physical address!
    shr eax, 12                      ; Get page index
    mov [trampoline_page], eax       ; Save for later

    ; Build page table
    xor eax, eax                     ; VA counter
    lea edi, [page_table]
    mov ecx, 1024

.pt_loop:
    mov edx, eax                     ; Start with VA
    mov ebx, eax
    shr ebx, 12                      ; Get page index

    ; Check if this is the trampoline page
    cmp ebx, [trampoline_page]
    je .identity_map

.offset_map:
    ; Normal offset mapping: VA + 1MB = PA
    add edx, KERNEL_BASE
    jmp .write_pte

.identity_map:
    ; Identity mapping: VA = PA
    ; edx already = VA, and for identity PA = VA
    ; Don't add KERNEL_BASE!

.write_pte:
    or  edx, 0x03                    ; present + RW
    mov [edi], edx

    add eax, 0x1000
    add edi, 4
    loop .pt_loop

    ; ------------------------------------------------------------
    ; Setup page directory
    ; ------------------------------------------------------------
    lea eax, [page_table]
    or  eax, 0x03
    mov [page_directory], eax

    lea eax, [page_directory]
    mov cr3, eax

    ; ------------------------------------------------------------
    ; Step 3: Enable paging (jump to trampoline first)
    ; ------------------------------------------------------------
    jmp paging_trampoline

; ------------------------------------------------------------
; Trampoline page (identity-mapped)
; ------------------------------------------------------------
align 4096
paging_trampoline:
    ; Verify trampoline PTE
    mov eax, [trampoline_page]
    shl eax, 2
    lea edi, [page_table]
    add edi, eax

    mov ebx, [edi]                   ; ebx = actual PTE value

    ; Write to VGA: "PTE: 0x12345678"
    mov edi, 0xB8000                 ; VGA text mode
    mov byte [edi], 'P'
    mov byte [edi+1], 0x0F           ; White on black
    mov byte [edi+2], 'T'
    mov byte [edi+3], 0x0F
    mov byte [edi+4], 'E'
    mov byte [edi+5], 0x0F
    mov byte [edi+6], ':'
    mov byte [edi+7], 0x0F
    mov byte [edi+8], ' '
    mov byte [edi+9], 0x0F
    mov byte [edi+10], '0'
    mov byte [edi+11], 0x0F
    mov byte [edi+12], 'x'
    mov byte [edi+13], 0x0F

    ; Print ebx as 8 hex digits
    mov edi, 0xB8000 + 14            ; Start after "0x"
    mov ecx, 8                       ; 8 hex digits

.print_hex:
    rol ebx, 4                       ; Rotate to get next nibble
    mov eax, ebx
    and eax, 0x0F                    ; Mask to get nibble

    cmp eax, 9
    jle .digit
    add eax, 'A' - 10                ; A-F
    jmp .write_char
.digit:
    add eax, '0'                     ; 0-9

.write_char:
    mov [edi], al                    ; Write character
    mov byte [edi+1], 0x0F           ; White on black
    add edi, 2
    loop .print_hex

    ; Hang to view PTE
.hang:
    hlt
    jmp .hang

    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax                     ; Paging is ON

    ; Step 4: Jump out of trampoline to offset-mapped region
    jmp post_paging

; Pad to next page boundary
align 4096

; ------------------------------------------------------------
; Step 5 & 6: Unmap trampoline, continue with kernel
; ------------------------------------------------------------
post_paging:
    ; Mark trampoline page as not present (garbage)
    mov eax, [trampoline_page]
    shl eax, 2                       ; Convert page index to byte offset
    lea edi, [page_table]
    add edi, eax                     ; edi = &page_table[trampoline_page]

    mov dword [edi], 0               ; Clear PTE (not present)

    ; Flush TLB
    mov eax, cr3
    mov cr3, eax

    ; Update stack to virtual address
    mov esp, KSTACK_TOP              ; Virtual stack

    ; Call kernel main
    call kmain

.hang:
    hlt
    jmp .hang

section .data
trampoline_page: dd 0