BITS 32

section .text

global ctx_switch
global ctx_init

extern task_trampoline

%define OFF_U_ESP  0
%define OFF_K_ESP  4

; ============================================================
; void ctx_init(struct cpu_ctx *cpu_ctx,
;               uint32_t entry_addr,
;               int argc,
;               char **heap_argv,
;               char **heap_envp);
; ============================================================
ctx_init:
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
    mov word [0xB8F9E], 0x0F2E    ; '.' - Clear/reset
    mov word [0xB8F9E], 0x0F41    ; 'A' - Entry
    cli

    mov eax, [esp + 4]             ; prev
    mov word [0xB8F9E], 0x0F42    ; 'B' - Loaded prev
    mov edx, [esp + 8]             ; next
    mov word [0xB8F9E], 0x0F43    ; 'C' - Loaded next
    mov ecx, [esp + 12]            ; vm_space
    mov word [0xB8F9E], 0x0F44    ; 'D' - Loaded vm_space

    ; Save current kernel ESP to prev->k_esp
    mov [eax + OFF_K_ESP], esp
    mov word [0xB8F9E], 0x0F45    ; 'E' - Saved prev k_esp

    ; Switch address space
    mov ecx, [ecx + 8]             ; vm_space->impl
    mov word [0xB8F9E], 0x0F46    ; 'F' - Loaded impl
    mov ecx, [ecx + 4]             ; impl->pd_pa
    mov word [0xB8F9E], 0x0F47    ; 'G' - Loaded pd_pa
    mov cr3, ecx
    mov word [0xB8F9E], 0x0F48    ; 'H' - Switched CR3

    ; Load next task's kernel ESP
    mov esp, [edx + OFF_K_ESP]
    mov word [0xB8F9E], 0x0F49    ; 'I' - Loaded k_esp

    ; Restore CPU state from kernel stack
    pop ebx
    mov word [0xB8F9E], 0x0F4A    ; 'J' - Popped ebx
    pop esi
    mov word [0xB8F9E], 0x0F4B    ; 'K' - Popped esi
    pop edi
    mov word [0xB8F9E], 0x0F4C    ; 'L' - Popped edi
    pop ebp
    mov word [0xB8F9E], 0x0F4D    ; 'M' - Popped ebp
    popfd
    mov word [0xB8F9E], 0x0F4E    ; 'N' - Popped eflags

    sti
    mov word [0xB8F9E], 0x0F4F    ; 'O' - STI done
    xor eax, eax
    ret