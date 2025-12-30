BITS 32

section .text

global ctx_switch
global ctx_setup_trampoline

extern task_trampoline

%define OFF_U_ESP  0
%define OFF_K_ESP  4

; ============================================================
; void ctx_setup_trampoline(struct cpu_ctx *cpu_ctx,
;                           uint32_t entry_addr,
;                           int argc,
;                           char **heap_argv,
;                           char **heap_envp);
; ============================================================
ctx_setup_trampoline:
    push ebp
    mov ebp, esp

    ; args (cdecl)
    ; [ebp+8]  = cpu_ctx
    ; [ebp+12] = entry_addr
    ; [ebp+16] = argc
    ; [ebp+20] = heap_argv
    ; [ebp+24] = heap_envp (not used)

    mov eax, [ebp+8]         ; cpu_ctx*
    mov edi, [eax + OFF_K_ESP]  ; Load kernel stack top

    ; Build kernel stack frame (working downward)
    ; Push arguments for task_trampoline (in reverse for cdecl)

    sub edi, 4
    mov edx, [ebp+20]
    mov [edi], edx           ; argv (3rd param)

    sub edi, 4
    mov edx, [ebp+16]
    mov [edi], edx           ; argc (2nd param)

    sub edi, 4
    mov edx, [ebp+12]
    mov [edi], edx           ; entry_addr (1st param)

    ; Dummy return address for task_trampoline's stack frame
    sub edi, 4
    mov dword [edi], 0

    ; Return address - where ctx_switch will ret to
    sub edi, 4
    mov edx, task_trampoline
    mov [edi], edx

    ; CPU state that ctx_switch will restore
    sub edi, 4
    mov dword [edi], 0x202   ; EFLAGS (IF=1)

    sub edi, 4
    mov dword [edi], 0       ; EBP

    sub edi, 4
    mov dword [edi], 0       ; EDI

    sub edi, 4
    mov dword [edi], 0       ; ESI

    sub edi, 4
    mov dword [edi], 0       ; EBX

    ; Save kernel ESP back to cpu_ctx
    mov [eax + OFF_K_ESP], edi

    pop ebp
    ret


; ============================================================
; void ctx_switch(struct cpu_ctx *prev,
;                 struct cpu_ctx *next,
;                 struct vm_space *vm_space);
; ============================================================
ctx_switch:
    cli
    mov eax, [esp + 4]      ; prev
    mov edx, [esp + 8]      ; next
    mov ecx, [esp + 12]     ; vm_space

    ; PUSH registers onto prev's stack BEFORE saving ESP
    push ebx
    push esi
    push edi
    push ebp
    pushfd

    ; NOW save ESP (points to saved registers)
    mov [eax + OFF_K_ESP], esp

    ; Switch page directory
    mov ecx, [ecx + 8]
    mov ecx, [ecx + 4]
    mov cr3, ecx

    ; Load next ESP and restore registers
    mov esp, [edx + OFF_K_ESP]

    popfd
    pop ebp
    pop edi
    pop esi
    pop ebx

    sti
    xor eax, eax
    ret