BITS 32

section .text

global sys_enter
global sys_return

extern sys_enter_dispatch_c
extern sched

; ============================================================
; Syscall entry point
; ============================================================
sys_enter:
    push esi
    push edx
    push ecx
    push ebx
    push eax

    pushfd
    push ebp
    push edi

    ; Get current task and save user ESP
    mov edi, [sched]            ; edi = sched.current
    mov [edi], esp              ; task->cpu_ctx.u_esp = esp

    ; Load kernel stack
    mov edi, [edi + 4]          ; edi = task->cpu_ctx.k_esp

    ; Copy args to kernel stack
    mov eax, [esp + 28]
    mov [edi - 4], eax
    mov eax, [esp + 24]
    mov [edi - 8], eax
    mov eax, [esp + 20]
    mov [edi - 12], eax
    mov eax, [esp + 16]
    mov [edi - 16], eax
    mov eax, [esp + 12]
    mov [edi - 20], eax

    lea esp, [edi - 20]

    call sys_enter_dispatch_c

    add esp, 20
    jmp sys_return

; ============================================================
; Common syscall return path
; ============================================================
sys_return:
    ; EAX contains return value
    ; We're on kernel stack

    ; Restore user ESP
    mov edi, [sched]            ; edi = sched.current
    mov esp, [edi]              ; esp = task->cpu_ctx.u_esp

    ; Restore callee-saved
    pop edi
    pop ebp
    popfd

    ; Discard saved syscall args
    add esp, 20

    ret