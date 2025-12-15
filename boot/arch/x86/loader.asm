; ========================================================================
;  loader.asm — verbose BIOS loader for PUnix using LBA INT 13h extensions
; ========================================================================

bits 16
org 0x7E00

; ------------------------------------------------------------------------
; Size helpers
; ------------------------------------------------------------------------
%define KB(x) (x * 1024)
%define MB(x) (x * 1024 * 1024)

; ------------------------------------------------------------------------
; Kernel layout
;   CMake layout:
;     sector 0 : bootsector
;     sectors 1–2 : loader
;     sector 3 : kernel.bin start
; ------------------------------------------------------------------------
%define SYS_CALLS_HDR_ADDR MB(1)              ; final run address (1 MiB)
%define KERNEL_LOAD_TEMP   KB(64)             ; we temporary load the kernel at addres 64KB
%define KERNEL_SECTORS     512                ; the kernel can now be up to 256 KB
%define KERNEL_SIZE_BYTES  (KERNEL_SECTORS * 512)
%define KERNEL_START_LBA   3                  ; kernel starts at LBA 3

; ========================================================================
; Boot entry
; ========================================================================
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    ; --------------------------------------------------------------------
    ; Clear full VGA text screen (white-on-black)
    ; --------------------------------------------------------------------
    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x0720            ; ' ' with attribute 0x07
    mov cx, 80 * 25
clear_screen:
    stosw
    loop clear_screen
    xor di, di                ; cursor at top-left

    ; --------------------------------------------------------------------
    ; Intro
    ; --------------------------------------------------------------------
    mov si, msg_intro
    call print_str
    call newline

    ; --------------------------------------------------------------------
    ; Reset disk
    ; --------------------------------------------------------------------
    mov si, msg_reset
    call print_str

    xor ax, ax
    mov dl, [boot_drive]
    int 0x13
    jc disk_reset_fail

    mov si, msg_ok
    call print_str
    call newline

    ; --------------------------------------------------------------------
    ; Read kernel with INT 13h extensions (LBA)
    ; --------------------------------------------------------------------
    mov si, msg_reading
    call print_str
    call newline

    ; init destination address (physical) and LBA
    mov dword [dest_phys], KERNEL_LOAD_TEMP
    mov dword [cur_lba],   KERNEL_START_LBA
    mov cx, KERNEL_SECTORS          ; sectors remaining (136)

read_lba_loop:
    ; batch size: at most 127 sectors per BIOS call (safe limit)
    cmp cx, 127
    jbe set_batch_last
    mov bx, 127
    jmp short set_batch_common
set_batch_last:
    mov bx, cx
set_batch_common:
    ; --------------------------------------------------------
    ; Prepare DAP for this batch
    ; --------------------------------------------------------
    ; Compute segment:offset of dest_phys
    mov eax, [dest_phys]        ; 32-bit physical address
    mov dx, ax                  ; low 16 bits
    and dx, 0x000F              ; offset = low 4 bits
    shr eax, 4                  ; segment = phys >> 4

    mov [dap_offset],  dx
    mov [dap_segment], ax
    mov [dap_sectors], bx

    mov eax, [cur_lba]
    mov [dap_lba_low],  eax
    mov dword [dap_lba_high], 0

    ; DS is 0, so SI = offset of dap is fine
    mov si, dap
    mov ah, 0x42                ; extended read
    mov dl, [boot_drive]
    int 0x13
    jc read_error

    ; --------------------------------------------------------
    ; Advance dest_phys += batch * 512
    ; --------------------------------------------------------
    xor eax, eax
    mov ax, bx                  ; batch count
    shl eax, 9                  ; * 512
    mov edx, [dest_phys]
    add edx, eax
    mov [dest_phys], edx

    ; cur_lba += batch
    mov eax, [cur_lba]
    xor edx, edx
    mov dx, bx
    add eax, edx
    mov [cur_lba], eax

    ; remaining sectors -= batch
    sub cx, bx
    jnz read_lba_loop

    ; done
    mov si, msg_done
    call print_str
    call newline
    jmp after_read

disk_reset_fail:
    mov si, msg_reset_fail
    call print_str
    call newline
    jmp hang

read_error:
    mov si, msg_read_fail
    call print_str
    call newline
    jmp hang

after_read:
    ; --------------------------------------------------------------------
    ; Enter protected mode
    ; --------------------------------------------------------------------
    mov si, msg_pm
    call print_str
    call newline

    cli
    call make_gdt_descriptor
    lgdt [gdt_ptr]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    jmp CODE_SEG:pm_start

; ========================================================================
; Helper routines (real mode)
; ========================================================================

; print_str: DS:SI -> ES:DI, prints null-terminated string
print_str:
    push ax
print_str_loop:
    lodsb
    test al, al
    jz print_str_done
    mov ah, 0x07
    stosw
    jmp print_str_loop
print_str_done:
    pop ax
    ret

; newline: move DI to start of next VGA row
; DI = byte offset, 2 bytes per cell, 80 cells per row
newline:
    push ax
    push bx
    push dx

    mov ax, di            ; ax = current byte offset
    shr ax, 1             ; cells = di / 2
    mov bx, 80
    xor dx, dx
    div bx                ; ax = row, dx = col
    inc ax                ; next row
    cmp ax, 25
    jb nl_ok_row
    mov ax, 24            ; clamp to last row
nl_ok_row:
    mov bx, 160           ; 80 columns * 2 bytes
    mul bx                ; DX:AX = row * 160
    mov di, ax            ; new byte offset at col 0

    pop dx
    pop bx
    pop ax
    ret

; build proper 6-byte GDT descriptor (limit + 32-bit base)
make_gdt_descriptor:
    mov ax, gdt_end - gdt_start - 1
    mov [gdt_ptr], ax             ; limit
    mov eax, gdt_start
    mov [gdt_ptr + 2], eax        ; base (32-bit)
    ret

; hang forever
hang:
    cli
    hlt
    jmp hang

; ========================================================================
; Stage 2 — Protected mode
; ========================================================================
bits 32
pm_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, KB(576)          ; reuse 0x00090000 as stack base

    ; copy kernel from temp to 1 MiB
    mov esi, KERNEL_LOAD_TEMP
    mov edi, SYS_CALLS_HDR_ADDR
    mov ecx, KERNEL_SIZE_BYTES / 4
    rep movsd

    ; jump to kernel entry at [1M + 4]
    mov eax, [SYS_CALLS_HDR_ADDR + 4]
    jmp eax

; ========================================================================
; GDT
; ========================================================================
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF       ; code segment
    dq 0x00CF92000000FFFF       ; data segment
gdt_end:

gdt_ptr:
    dw 0                        ; limit
    dd 0                        ; base

CODE_SEG equ 0x08
DATA_SEG equ 0x10

; ========================================================================
; Disk Address Packet (for INT 13h AH=42h)
; ========================================================================
dap:
    db 0x10                     ; size of packet (16 bytes)
    db 0x00                     ; reserved
dap_sectors:
    dw 0                        ; number of sectors for this call
dap_offset:
    dw 0                        ; buffer offset
dap_segment:
    dw 0                        ; buffer segment
dap_lba_low:
    dd 0                        ; starting LBA (low dword)
dap_lba_high:
    dd 0                        ; starting LBA (high dword, unused here)

dest_phys   dd 0                ; current physical destination address
cur_lba     dd 0                ; current LBA

; ========================================================================
; Messages
; ========================================================================
msg_intro      db 'PUnix loader starting...',0
msg_reset      db 'Resetting disk... ',0
msg_reset_fail db 'Disk reset failed!',0
msg_ok         db 'OK',0
msg_reading    db 'Reading kernel from disk (LBA)...',0
msg_done       db 'Kernel read complete.',0
msg_read_fail  db 'Disk read failed!',0
msg_pm         db 'Entering protected mode...',0

boot_drive db 0

times 1024 - ($ - $$) db 0
