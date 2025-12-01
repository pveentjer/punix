; kernel.asm
bits 16
org 0x0000

start:
    mov ax, cs
    mov ds, ax
    mov es, ax

    mov si, msg
.print:
    lodsb
    or  al, al
    jz  .done
    mov ah, 0x0E
    int 0x10
    jmp .print

.done:
    cli
.halt:
    hlt
    jmp .halt

msg db "Munix 0.001", 0
times 1024-($-$$) db 0     ; pad kernel to 2 sectors (1024 bytes)

