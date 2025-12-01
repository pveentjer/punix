; loader.asm
bits 16
org 0x7E00

start:
    mov ax, cs
    mov ds, ax
    mov es, ax

    mov [boot_drive], dl

    mov ax, 0x1000
    mov es, ax
    mov bx, 0x0000
    mov ah, 0x02
    mov al, 2              ; load 2 sectors for kernel
    mov ch, 0
    mov cl, 4              ; sector 4 (LBA index 3 zero-based)
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp 0x1000:0x0000

disk_error:
    mov si, msg
    mov ah, 0x0E
.errloop:
    lodsb
    or al, al
    jz .halt
    int 0x10
    jmp .errloop
.halt:
    cli
.hang: hlt
    jmp .hang

msg db "Loader read error!",0
boot_drive db 0
times 1024-($-$$) db 0     ; pad loader to 2 sectors (1024 bytes)

