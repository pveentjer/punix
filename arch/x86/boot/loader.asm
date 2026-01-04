; ========================================================================
;  loader.asm — chunked loader for PUnix
; ========================================================================

bits 16
org 0x7E00

; ------------------------------------------------------------------------
; Size helpers
; ------------------------------------------------------------------------
%define KB(x) (x * 1024)
%define MB(x) (x * 1024 * 1024)

; THe size of the image.
%define IMAGE_SIZE           MB(4)
; Size of a single sector
%define SECTOR_SIZE          512
; The number of sectors required to hold the image
%define IMAGE_SECTORS        (IMAGE_SIZE / SECTOR_SIZE)
; final run address (1 MiB)
%define KERNEL_LOAD_ADDR      MB(1)
; temporary load address (64KB)
%define KERNEL_LOAD_TEMP      KB(64)
; The loader is 2 sectors (2x 512B)
%define LOADER_SECTORS        2
; kernel starts at LBA 3
%define KERNEL_START_LBA      (1 + LOADER_SECTORS)
; The number of kernel sectors. We don't need to copy the bootsector/loader sectors
%define KERNEL_SECTORS        (IMAGE_SECTORS - KERNEL_START_LBA)
; The of the intermediate buffer
%define CHUNK_SIZE            KB(64)
; sectors per chunk
%define CHUNK_SECTORS         (CHUNK_SIZE / SECTOR_SIZE)
; kernel virtual address stack at 2MB
%define KERNEL_STACK_TOP_VA   MB(2)

%define PREMAIN_PA            KERNEL_LOAD_ADDR   ; premain at offset 0

%define VGA                   0xB800

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
    ; Clear full VGA text screen
    ; --------------------------------------------------------------------
    mov ax, VGA
    mov es, ax
    xor di, di
    mov ax, 0x0720            ; ' ' with attribute 0x07
    mov cx, 80 * 25
clear_screen:
    stosw
    loop clear_screen
    xor di, di

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
    ; Chunked kernel loading with progress
    ; --------------------------------------------------------------------
    mov si, msg_reading
    call print_str
    call newline

    mov si, msg_progress
    call print_str

    ; Save current VGA cursor position for dot printing
    mov ax, di
    shr ax, 1                           ; convert to character position
    movzx eax, ax                       ; zero-extend to 32-bit
    mov [vga_cursor], eax               ; store as dword

    ; Initialize chunk loop
    mov dword [cur_lba], KERNEL_START_LBA
    mov word [sectors_remaining], KERNEL_SECTORS
    mov dword [dest_offset], 0

chunk_loop:
    ; --------------------------------------------------------
    ; Determine chunk size
    ; --------------------------------------------------------
    mov cx, [sectors_remaining]
    cmp cx, CHUNK_SECTORS
    jbe last_chunk
    mov cx, CHUNK_SECTORS
last_chunk:
    mov [current_chunk_size], cx

    ; --------------------------------------------------------
    ; Read chunk to temp buffer
    ; --------------------------------------------------------
    call read_chunk_to_temp
    jc read_error

    ; --------------------------------------------------------
    ; Switch to protected mode and copy chunk
    ; --------------------------------------------------------
    cli
    call make_gdt_descriptor
    lgdt [gdt_ptr]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    jmp CODE_SEG:copy_chunk_pm

copy_chunk_return:
    ; Back from protected mode

    ; --------------------------------------------------------
    ; Update for next chunk
    ; --------------------------------------------------------
    xor eax, eax
    mov ax, [current_chunk_size]

    ; dest_offset += chunk_size * 512
    shl eax, 9                          ; * 512
    add [dest_offset], eax

    ; cur_lba += chunk_size
    shr eax, 9                          ; back to sectors
    add [cur_lba], eax

    ; sectors_remaining -= chunk_size
    mov ax, [current_chunk_size]
    sub [sectors_remaining], ax

    ; Continue if more sectors remain
    cmp word [sectors_remaining], 0
    jne chunk_loop

    ; --------------------------------------------------------
    ; All chunks loaded - move to new line
    ; --------------------------------------------------------
    mov ax, VGA
    mov es, ax
    mov eax, [vga_cursor]
    shl eax, 1                          ; convert back to byte offset
    mov di, ax
    call newline

    mov si, msg_done
    call print_str
    call newline

    ; --------------------------------------------------------
    ; Enter protected mode for final hang
    ; --------------------------------------------------------
    mov si, msg_pm
    call print_str
    call newline

    cli
    call make_gdt_descriptor
    lgdt [gdt_ptr]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    jmp CODE_SEG:kernel_start

; ========================================================================
; read_chunk_to_temp - Read current chunk to KERNEL_LOAD_TEMP
; ========================================================================
read_chunk_to_temp:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    mov cx, [current_chunk_size]       ; total sectors for this chunk
    xor di, di                          ; byte offset into temp buffer
    xor si, si                          ; sector offset within chunk

.read_loop:
    ; Determine batch size (max 127 sectors)
    cmp cx, 127
    jbe .last_batch
    mov bx, 127
    jmp .do_batch
.last_batch:
    mov bx, cx

.do_batch:
    ; Calculate physical destination address: KERNEL_LOAD_TEMP + di
    mov eax, KERNEL_LOAD_TEMP
    movzx edx, di
    add eax, edx

    ; Convert to segment:offset
    mov dx, ax
    and dx, 0x000F                      ; offset = low 4 bits
    shr eax, 4                          ; segment = addr >> 4

    mov [dap_offset], dx
    mov [dap_segment], ax
    mov [dap_sectors], bx

    ; LBA = cur_lba + sectors_read_so_far_in_chunk
    mov eax, [cur_lba]
    movzx edx, si
    add eax, edx
    mov [dap_lba_low], eax
    mov dword [dap_lba_high], 0

    ; Execute read
    push si
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    pop si
    jc .error

    ; Advance offset within chunk
    add si, bx

    ; Advance offset in temp buffer
    mov ax, bx
    shl ax, 9                           ; * 512
    add di, ax

    ; Continue batch loop
    sub cx, bx
    jnz .read_loop

    clc                                 ; success
    jmp .done

.error:
    stc                                 ; error flag

.done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ========================================================================
; Helper routines (real mode)
; ========================================================================

print_str:
    push ax
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x07
    stosw
    jmp .loop
.done:
    pop ax
    ret

newline:
    push ax
    push bx
    push dx

    mov ax, di
    shr ax, 1
    mov bx, 80
    xor dx, dx
    div bx
    inc ax
    cmp ax, 25
    jb .ok_row
    mov ax, 24
.ok_row:
    mov bx, 160
    mul bx
    mov di, ax

    pop dx
    pop bx
    pop ax
    ret

make_gdt_descriptor:
    mov ax, gdt_end - gdt_start - 1
    mov [gdt_ptr], ax
    mov eax, gdt_start
    mov [gdt_ptr + 2], eax
    ret

hang:
    cli
    hlt
    jmp hang

disk_reset_fail:
    mov si, msg_reset_fail
    call print_str
    call newline
    jmp hang

read_error:
    mov ax, VGA
    mov es, ax
    mov eax, [vga_cursor]
    shl eax, 1
    mov di, ax
    call newline
    mov si, msg_read_fail
    call print_str
    call newline
    jmp hang

; ========================================================================
; Protected mode code
; ========================================================================
bits 32

; Copy current chunk from temp to final location and print dot
copy_chunk_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Calculate copy size
    xor ecx, ecx
    mov cx, [current_chunk_size]
    shl ecx, 9                          ; * 512
    shr ecx, 2                          ; / 4 for dwords

    ; Setup copy
    mov esi, KERNEL_LOAD_TEMP
    mov edi, KERNEL_LOAD_ADDR
    add edi, [dest_offset]

    rep movsd

    ; Print progress dot to VGA
    mov edi, [vga_cursor]               ; load character position
    mov ax, 0x0E2E                      ; bright white dot
    mov [0xB8000 + edi*2], ax
    inc edi
    mov [vga_cursor], edi

    ; Return to real mode
    jmp CODE_SEG_16:return_to_real

bits 16
return_to_real:
    mov ax, DATA_SEG_16
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, cr0
    and eax, 0xFFFFFFFE                 ; clear PE bit
    mov cr0, eax

    jmp 0x0000:copy_chunk_return_real

copy_chunk_return_real:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    jmp copy_chunk_return

bits 32
kernel_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, KERNEL_STACK_TOP_VA

    jmp PREMAIN_PA

.loop:
    hlt
    jmp .loop

; ========================================================================
; GDT
; ========================================================================
%macro GDT_ENTRY 4
    dw  %2 & 0xFFFF
    dw  %1 & 0xFFFF
    db  (%1 >> 16) & 0xFF
    db  %3
    db  ((%2 >> 16) & 0x0F) | ((%4 & 0x0F) << 4)
    db  (%1 >> 24) & 0xFF
%endmacro

%define GDT_FLAGS_FLAT     0xC
%define GDT_ACCESS_CODE    0x9A
%define GDT_ACCESS_DATA    0x92

bits 16
gdt_start:
    dq 0
    GDT_ENTRY 0, 0x000FFFFF, GDT_ACCESS_CODE, GDT_FLAGS_FLAT    ; 32-bit code
    GDT_ENTRY 0, 0x000FFFFF, GDT_ACCESS_DATA, GDT_FLAGS_FLAT    ; 32-bit data
    GDT_ENTRY 0, 0x0000FFFF, 0x9A, 0x0                          ; 16-bit code
    GDT_ENTRY 0, 0x0000FFFF, 0x92, 0x0                          ; 16-bit data
gdt_end:

gdt_ptr:
    dw 0
    dd 0

CODE_SEG    equ 0x08
DATA_SEG    equ 0x10
CODE_SEG_16 equ 0x18
DATA_SEG_16 equ 0x20

; ========================================================================
; Disk Address Packet
; ========================================================================
dap:
    db 0x10
    db 0x00
dap_sectors:
    dw 0
dap_offset:
    dw 0
dap_segment:
    dw 0
dap_lba_low:
    dd 0
dap_lba_high:
    dd 0

; ========================================================================
; Variables
; ========================================================================
cur_lba              dd 0
dest_offset          dd 0
sectors_remaining    dw 0
current_chunk_size   dw 0
vga_cursor           dd 0
boot_drive           db 0

; ========================================================================
; Messages
; ========================================================================
msg_intro       db 'PUnix chunked loader starting...',0
msg_reset       db 'Resetting disk... ',0
msg_reset_fail  db 'Disk reset failed!',0
msg_ok          db 'OK',0
msg_reading     db 'Reading kernel from disk (2MB, chunked):',0
msg_progress    db 'Progress: ',0
msg_done        db 'Kernel load complete (32 chunks).',0
msg_read_fail   db 'Disk read failed!',0
msg_pm          db 'Hanging in protected mode...',0

times 1024 - ($ - $$) db 0