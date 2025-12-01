; sched_x86.asm
BITS 32

section .text

global task_start
global task_context_switch

; struct task_struct layout (offsets):
;  0  pid
;  4  eip
;  8  esp
; 12  ebp
; 16  eflags
; ... next, started not used here

%define OFF_EIP  4
%define OFF_ESP  8
%define OFF_EBP 12

; void task_start(struct task_struct *t);
; First time a task runs.
task_start:
    mov eax, [esp + 4]        ; eax = t
    mov edx, [eax + OFF_EIP]  ; edx = t->eip
    mov esp, [eax + OFF_ESP]  ; esp = t->esp
    mov ebp, [eax + OFF_EBP]  ; ebp = t->ebp
    jmp edx                   ; never returns


; int task_context_switch(struct task_struct *current,
;                         struct task_struct *next);
task_context_switch:
    ; save current context on its stack
    pushad
    pushfd

    ; stack layout now:
    ; esp+0  EFLAGS
    ; esp+4  EDI
    ; esp+8  ESI
    ; esp+12 EBP
    ; esp+16 saved ESP
    ; esp+20 EBX
    ; esp+24 EDX
    ; esp+28 ECX
    ; esp+32 EAX
    ; esp+36 RETADDR (back into yield)
    ; esp+40 current
    ; esp+44 next

    mov eax, [esp + 40]       ; eax = current
    mov [eax + OFF_ESP], esp  ; current->esp = esp

    ; load next context
    mov eax, [esp + 44]       ; eax = next
    mov esp, [eax + OFF_ESP]  ; switch to its stack

    popfd
    popad
    ret                       ; back into next's yield caller
