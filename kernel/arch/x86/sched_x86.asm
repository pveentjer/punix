; sched_x86.asm
BITS 32

section .text

global ctx_switch

%define OFF_EIP  4
%define OFF_ESP  8
%define OFF_EBP 12
             ; jump to task entry


; int ctx_switch(struct task *prev, struct task *next);
; Switch from prev to next task
ctx_switch:
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
    
    ; --- Force IF=1 safely ---
    sti                       ; set Interrupt Flag directly
    ; -------------------------

    xor eax, eax              ; return 0
    ret                       ; return to next task