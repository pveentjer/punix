; loader.asm - Stage 2: print "Munix 0.001" and halt
; Assemble: nasm -f bin loader.asm -o loader.bin

    bits 16
    org 0x7E00                 ; where stage 1 loaded us

start:
    ; Set DS/ES to our code segment
    mov ax, cs
    mov ds, ax
    mov es, ax

    mov si, msg

.print_loop:
    lodsb                      ; AL = [DS:SI], SI++
    or  al, al
    jz  .done

    mov ah, 0x0E               ; BIOS teletype output
    mov bh, 0x00
    mov bl, 0x07               ; light gray on black
    int 0x10

    jmp .print_loop

.done:
    cli
.hang:
    hlt
    jmp .hang

msg db "Munix 0.001", 0

