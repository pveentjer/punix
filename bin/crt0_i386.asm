; bin/crt0_i386.asm  (NASM, elf32)
BITS 32

global _start
extern main
extern exit

section .text

_start:
    cld                     ; DF=0

    ; stack on entry:
    ;   [esp+0]  argc
    ;   [esp+4]  argv[0]
    ;   ...
    ;   argv[argc] = 0
    ;   envp[0]
    ;   ...
    ;   envp[n] = 0
    ;   auxv...

    mov ecx, [esp]          ; ecx = argc
    lea eax, [esp + 4]      ; eax = argv (char**)

    ; envp = &argv[argc+1] = esp + 4 + (argc+1)*4 = esp + 8 + argc*4
    lea edx, [esp + 8 + ecx*4]   ; edx = envp (char**)

    push edx                ; envp
    push eax                ; argv
    push ecx                ; argc
    call main

    push eax                ; status
    call exit

.hang:
    hlt
    jmp .hang
