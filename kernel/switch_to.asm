; switch_to(struct task_struct *t)
bits 32
global switch_to
switch_to:
    mov eax, [esp + 4]      ; eax = pointer to task_struct
    mov esp, [eax + 8]      ; load esp  (offset 8 = esp)
    mov ebp, [eax + 12]     ; load ebp  (offset 12 = ebp)
    mov eax, [eax + 4]      ; load eip  (offset 4 = eip)
    jmp eax                 ; jump to entry point (task_entry)
