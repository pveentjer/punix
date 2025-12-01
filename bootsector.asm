; bootsector.asm - Stage 1: load second-stage bootloader
; Assemble: nasm -f bin bootsector.asm -o bootsector.bin

    bits 16
    org 0x7C00                 ; BIOS load address for boot sector

start:
    cli
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00             ; simple stack
    sti

    mov [boot_drive], dl       ; BIOS passes boot drive in DL

    ; Make DS/ES point to our code/data segment
    mov ax, cs
    mov ds, ax
    mov es, ax

    ; ------------------------------------------------------------
    ; Load second stage (sector 2) to 0x0000:0x7E00
    ; ------------------------------------------------------------
    mov ax, 0x0000
    mov es, ax                 ; ES = 0x0000
    mov bx, 0x7E00             ; ES:BX = destination

    mov ah, 0x02               ; INT 13h - read sectors
    mov al, 1                  ; read 1 sector (512 bytes)
    mov ch, 0                  ; cylinder 0
    mov cl, 2                  ; sector 2 (1-based)
    mov dh, 0                  ; head 0
    mov dl, [boot_drive]       ; drive number

    int 0x13                   ; BIOS disk read
    jc disk_error              ; if carry set -> error

    jmp 0x0000:0x7E00          ; jump to second stage

; ------------------------------------------------------------
; Error handler: print "Disk read error!" and hang
; ------------------------------------------------------------
disk_error:
    mov ax, cs
    mov ds, ax

    mov si, err_msg
.print_err:
    lodsb
    or al, al
    jz .hang
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp .print_err

.hang:
    cli
.hlt_loop:
    hlt
    jmp .hlt_loop

; ------------------------------------------------------------
; Data
; ------------------------------------------------------------
boot_drive db 0
err_msg    db "Disk read error!", 0

; ------------------------------------------------------------
; Pad to 512 bytes and add boot signature
; ------------------------------------------------------------
    times 510-($-$$) db 0
    dw 0xAA55                 ; boot signature

