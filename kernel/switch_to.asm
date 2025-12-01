BITS 32

section .text

global task_start
global task_context_store
global task_context_load

%define OFF_EIP  4
%define OFF_ESP  8
%define OFF_EBP  12

; ------------------------------------------------------------
; void task_start(struct task_struct *t);
; First time a task runs: use t->esp as stack and jump to t->eip
; ------------------------------------------------------------
task_start:
    mov eax, [esp + 4]          ; eax = t

    mov edx, [eax + OFF_EIP]    ; edx = t->eip
    mov esp, [eax + OFF_ESP]    ; esp = t->esp
    mov ebp, [eax + OFF_EBP]    ; ebp = t->ebp (0 is fine first time)

    jmp edx                     ; never returns


; ------------------------------------------------------------
; int task_context_store(struct task_struct *t);
;
; Save ESP/EBP + a resume EIP into t, and return:
;   0 when we are *leaving* this task (on yield/schedule-out)
;   1 when we are *resuming* this task (after context_load)
; ------------------------------------------------------------
task_context_store:
    push ebp
    mov  ebp, esp

    mov  eax, [ebp + 8]         ; eax = t

    ; Save current stack/frame
    mov  [eax + OFF_ESP], esp
    mov  [eax + OFF_EBP], ebp

    ; Save the address we want to resume at
    mov  dword [eax + OFF_EIP], .resume

    xor  eax, eax               ; return 0 the first time
    jmp  .end

.resume:
    mov  eax, 1                 ; when resumed, we return 1

.end:
    pop  ebp
    ret


; ------------------------------------------------------------
; void task_context_load(struct task_struct *t);
; Restore ESP/EBP and jump to saved EIP. Never returns.
; ------------------------------------------------------------
task_context_load:
    mov eax, [esp + 4]          ; eax = t

    mov esp, [eax + OFF_ESP]
    mov ebp, [eax + OFF_EBP]
    jmp dword [eax + OFF_EIP]   ; jump to resume point
