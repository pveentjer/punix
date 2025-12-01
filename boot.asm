; boot.asm - 512-byte boot sector

    bits 16
    org 0x7C00              ; BIOS loads boot sector here

start:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov si, msg             ; DS:SI -> string

.print_loop:
    lodsb                   ; AL = [SI], SI++
    cmp al, 0
    je .done

    mov ah, 0x0E            ; BIOS teletype output
    mov bh, 0x00            ; page number
    mov bl, 0x07            ; text attribute (light gray on black)
    int 0x10                ; call BIOS
    jmp .print_loop

.done:
    cli
.hang:
    hlt                     ; stop CPU until next interrupt
    jmp .hang

msg db "Munix 0.001!", 0   ; zero-terminated string

; ------------------------------------------------------------------
; Pad to 512 bytes and add boot signature 0xAA55 at the end
; ------------------------------------------------------------------
    times 510-($-$$) db 0
    dw 0xAA55               ; boot signature
