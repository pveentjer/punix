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

    ; Push arguments for task_trampoline (deepest on stack)
    sub  ecx, 4
    mov  edx, [ebp+28]
    mov  [ecx], edx          ; heap_envp

    sub  ecx, 4
    mov  edx, [ebp+24]
    mov  [ecx], edx          ; heap_argv

    sub  ecx, 4
    mov  edx, [ebp+20]
    mov  [ecx], edx          ; argc

    sub  ecx, 4
    mov  edx, [ebp+16]
    mov  [ecx], edx          ; main_addr

    sub  ecx, 4
    mov  dword [ecx], 0      ; dummy

    ; Return address - where ctx_switch will ret to
    sub  ecx, 4
    mov  edx, task_trampoline
    mov  [ecx], edx

    ; CPU state that ctx_switch will restore (must match pop order!)
    sub  ecx, 4
    mov  dword [ecx], 0x202  ; EFLAGS (IF=1)

    sub  ecx, 4
    mov  dword [ecx], 0      ; EBP

    sub  ecx, 4
    mov  dword [ecx], 0      ; EDI

    sub  ecx, 4
    mov  dword [ecx], 0      ; ESI

    sub  ecx, 4
    mov  dword [ecx], 0      ; EBX

    ; Save ESP to cpu_ctx
    mov  eax, [ebp+8]        ; cpu_ctx*
    mov  [eax + OFF_ESP], ecx

    pop  ebp
    ret


; ============================================================
; int ctx_switch(struct cpu_ctx *prev,
;                struct cpu_ctx *next,
;                struct vm_space *vm_space);
; ============================================================
ctx_switch:
    mov word [0xB8F00], 0x0730  ; '0' at bottom left corner

    cli

    ; Stack layout:
    ; esp+4  = prev (ignored)
    ; esp+8  = next
    ; esp+12 = vm_space

    mov edx, [esp + 8]      ; next
    mov ecx, [esp + 12]     ; vm_space

    ; Switch address space
    mov ecx, [ecx + 8]      ; vm_space->impl
    mov ecx, [ecx + 4]      ; impl->pd_pa
    mov cr3, ecx

    ; Load next task's user ESP (points to saved CPU state)
    mov esp, [edx + OFF_ESP]

    ; Restore CPU state from user stack
    pop ebx
    pop esi
    pop edi
    pop ebp
    popfd

    sti
    xor eax, eax
    mov word [0xB8F02], 0x0731  ; '1' next to it
    ret

