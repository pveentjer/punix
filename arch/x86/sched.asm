; sched.asm
BITS 32

section .text

global ctx_switch
global ctx_init

extern task_trampoline

%define OFF_ESP  0
%define OFF_DS   8
%define OFF_SS   4

; ============================================================
; void ctx_init(struct cpu_ctx *cpu_ctx,
;               uint32_t stack_top,
;               uint32_t main_addr,
;               int argc,
;               char **heap_argv,
;               char **heap_envp);
; ============================================================
ctx_init:
    push ebp
    mov  ebp, esp

    mov  eax, [ebp+12]
    mov  ecx, eax

    sub  ecx, 4
    mov  edx, [ebp+28]
    mov  [ecx], edx

    sub  ecx, 4
    mov  edx, [ebp+24]
    mov  [ecx], edx

    sub  ecx, 4
    mov  edx, [ebp+20]
    mov  [ecx], edx

    sub  ecx, 4
    mov  edx, [ebp+16]
    mov  [ecx], edx

    sub  ecx, 4
    mov  dword [ecx], 0

    sub  ecx, 4
    mov  edx, task_trampoline
    mov  [ecx], edx

    sub  ecx, 4
    mov  dword [ecx], 0x202

    sub  ecx, 4
    mov  dword [ecx], 0

    sub  ecx, 4
    mov  dword [ecx], 0

    sub  ecx, 4
    mov  dword [ecx], 0

    sub  ecx, 4
    mov  dword [ecx], 0

    mov  eax, [ebp+8]
    mov  [eax + OFF_ESP], ecx

    pop  ebp
    ret


; ============================================================
; int ctx_switch(struct cpu_ctx *prev, struct cpu_ctx *next);
; ============================================================
ctx_switch:
    cli

    pushfd
    push ebp
    push edi
    push esi
    push ebx

    mov eax, [esp + 24]
    mov edx, [esp + 28]

    mov [eax + OFF_ESP], esp
    mov esp, [edx + OFF_ESP]

    pop ebx
    pop esi
    pop edi
    pop ebp

    mov ax, [edx + OFF_DS]
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ax, [edx + OFF_SS]
    mov ss, ax

    popfd
    sti
    xor eax, eax
    ret
