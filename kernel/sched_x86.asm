; sched_x86.asm
BITS 32

section .text

global task_context_switch
global task_prepare_new

%define OFF_EIP  4
%define OFF_ESP  8
%define OFF_EBP 12
             ; jump to task entry


; void task_prepare_new(struct task_struct *t);
; Prepare a new task's stack to look like it was context-switched
; so we can use task_context_switch to start it
task_prepare_new:
    mov eax, [esp + 4]        ; eax = t
    mov ecx, [eax + OFF_ESP]  ; ecx = t->esp (top of its fresh stack)
    
    ; Build a fake context switch frame on the task's stack
    ; We'll push (from top to bottom):
    ; - return address (task entry point)
    ; - saved EBX (0)
    ; - saved ESI (0)
    ; - saved EDI (0)
    ; - saved EBP (initial EBP)
    ; - saved EFLAGS (0x202)
    
    mov edx, [eax + OFF_EIP]  ; task entry point
    sub ecx, 4
    mov [ecx], edx            ; return address = entry point
    
    sub ecx, 4
    mov dword [ecx], 0        ; EBX = 0
    
    sub ecx, 4
    mov dword [ecx], 0        ; ESI = 0
    
    sub ecx, 4
    mov dword [ecx], 0        ; EDI = 0
    
    mov edx, [eax + OFF_EBP]  ; initial EBP
    sub ecx, 4
    mov [ecx], edx            ; saved EBP
    
    sub ecx, 4
    mov dword [ecx], 0x202    ; EFLAGS with interrupts enabled
    
    ; Save the new ESP back
    mov [eax + OFF_ESP], ecx
    ret


; int task_context_switch(struct task_struct *prev, struct task_struct *next);
; Switch from prev to next task
task_context_switch:
    ; Save callee-saved registers and flags
    pushfd
    push ebp
    push edi
    push esi
    push ebx
    
    ; Stack layout now:
    ; esp+0:  EBX
    ; esp+4:  ESI
    ; esp+8:  EDI
    ; esp+12: EBP
    ; esp+16: EFLAGS
    ; esp+20: return address
    ; esp+24: prev pointer
    ; esp+28: next pointer
    
    mov eax, [esp + 24]       ; eax = prev
    mov edx, [esp + 28]       ; edx = next
    
    ; Save current ESP into prev task
    mov [eax + OFF_ESP], esp
    
    ; Switch to next task's stack
    mov esp, [edx + OFF_ESP]
    
    ; Restore next task's registers (pops in reverse order)
    pop ebx
    pop esi
    pop edi
    pop ebp
    popfd
    
    xor eax, eax              ; return 0
    ret                       ; return to next task