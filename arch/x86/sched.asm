BITS 32

section .text

global ctx_switch
global ctx_init

extern task_trampoline

%define OFF_ESP  0

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

    ; args (cdecl)
    ; [ebp+8]  = cpu_ctx
    ; [ebp+12] = stack_top
    ; [ebp+16] = main_addr
    ; [ebp+20] = argc
    ; [ebp+24] = heap_argv
    ; [ebp+28] = heap_envp

    mov  eax, [ebp+12]       ; stack_top
    mov  ecx, eax            ; working stack pointer

    ; *(--sp32) = (uint32_t) heap_envp;
    sub  ecx, 4
    mov  edx, [ebp+28]
    mov  [ecx], edx

    ; *(--sp32) = (uint32_t) heap_argv;
    sub  ecx, 4
    mov  edx, [ebp+24]
    mov  [ecx], edx

    ; *(--sp32) = (uint32_t) argc;
    sub  ecx, 4
    mov  edx, [ebp+20]
    mov  [ecx], edx

    ; *(--sp32) = main_addr;
    sub  ecx, 4
    mov  edx, [ebp+16]
    mov  [ecx], edx

    ; *(--sp32) = 0;  // dummy
    sub  ecx, 4
    mov  dword [ecx], 0

    ; *(--sp32) = (uint32_t) task_trampoline;  // return address
    sub  ecx, 4
    mov  edx, task_trampoline
    mov  [ecx], edx

    ; *(--sp32) = 0x202;  // EFLAGS IF=1
    sub  ecx, 4
    mov  dword [ecx], 0x202

    ; *(--sp32) = 0;  // EAX
    sub  ecx, 4
    mov  dword [ecx], 0

    ; *(--sp32) = 0;  // ECX
    sub  ecx, 4
    mov  dword [ecx], 0

    ; *(--sp32) = 0;  // EDX
    sub  ecx, 4
    mov  dword [ecx], 0

    ; *(--sp32) = 0;  // EBX
    sub  ecx, 4
    mov  dword [ecx], 0

    ; cpu_ctx->esp = (uint32_t) sp32;
    mov  eax, [ebp+8]        ; cpu_ctx*
    mov  [eax + OFF_ESP], ecx

    pop  ebp
    ret


; ============================================================
; int ctx_switch(struct cpu_ctx *prev, struct cpu_ctx *next);
; ============================================================
ctx_switch:
    cli

    ; Save callee-saved registers and flags on OLD stack
    pushfd
    push ebp
    push edi
    push esi
    push ebx

    ; esp+24 = prev pointer
    ; esp+28 = next pointer
    mov eax, [esp + 24]      ; prev
    mov edx, [esp + 28]      ; next

    ; Save current ESP into prev->cpu_ctx.esp
    mov [eax + OFF_ESP], esp

    ; Load next ESP
    mov esp, [edx + OFF_ESP]

    ; Restore next task's registers from NEW stack
    pop ebx
    pop esi
    pop edi
    pop ebp

    ; Restore next task's flags
    popfd

    sti
    xor eax, eax
    ret
