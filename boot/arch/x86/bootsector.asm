; bootsector.asm
bits 16
org 0x7C00

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    mov bx, 0x7E00         ; destination address
    mov ah, 0x02           ; BIOS read sectors
    mov al, 2              ; read 2 sectors (loader size)
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp 0x0000:0x7E00

disk_error:
    mov si, msg
.print:
    lodsb
    or al, al
    jz .halt
    mov ah, 0x0E
    int 0x10
    jmp .print
.halt:
    cli
.hang: hlt
    jmp .hang

msg db "Disk read error!",0
boot_drive db 0
times 510-($-$$) db 0
dw 0xAA55

