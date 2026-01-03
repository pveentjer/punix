BITS 32

section .text

global ctx_switch
global ctx_setup_trampoline
global task_trampoline
global ctx_setup_fork_return
extern sys_return

extern sched_current
extern vfs_open
extern sched_exit
extern vfs

%define OFF_U_ESP  0
%define OFF_K_ESP  4
%define O_RDONLY   0
%define O_WRONLY   1

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
; void task_trampoline(int (*entry)(int, char **), int argc, char **argv)
; ============================================================
task_trampoline:
    push ebp
    mov ebp, esp
    sub esp, 16              ; Space for locals
    push ebx
    push esi
    push edi

    ; Save arguments
    mov eax, [ebp + 8]       ; entry
    mov [ebp - 4], eax
    mov eax, [ebp + 12]      ; argc
    mov [ebp - 8], eax
    mov eax, [ebp + 16]      ; argv
    mov [ebp - 12], eax

    ; Get current task
    call sched_current
    mov [ebp - 16], eax      ; save current

    ; vfs_open(current, "/dev/stdin", O_RDONLY, 0)
    push dword 0
    push dword O_RDONLY
    push .stdin_str
    push eax
    call vfs_open
    add esp, 16

    mov eax, [ebp - 16]

    ; vfs_open(current, "/dev/stdout", O_WRONLY, 0)
    push dword 0
    push dword O_WRONLY
    push .stdout_str
    push eax
    call vfs_open
    add esp, 16

    mov eax, [ebp - 16]

    ; vfs_open(current, "/dev/stderr", O_WRONLY, 0)
    push dword 0
    push dword O_WRONLY
    push .stderr_str
    push eax
    call vfs_open
    add esp, 16

    ; Get current and u_sp
    mov edi, [ebp - 16]      ; current
    mov ecx, [edi]           ; u_sp (first field in cpu_ctx)

    ; Save kernel ESP
    mov ebx, esp
    mov [edi + 4], ebx       ; save to k_esp

    ; Switch to user stack
    mov esp, ecx

    ; Push arguments
    push dword [ebp - 12]    ; argv
    push dword [ebp - 8]     ; argc

    ; Call entry
    call [ebp - 4]
    add esp, 8

    ; Restore kernel stack
    mov esp, ebx

    ; Call sched_exit(exit_code)
    push eax
    call sched_exit
    ; Never returns

section .rodata
.stdin_str:  db '/dev/stdin', 0
.stdout_str: db '/dev/stdout', 0
.stderr_str: db '/dev/stderr', 0

section .text

; ============================================================
; void ctx_switch(struct cpu_ctx *prev,
;                 struct cpu_ctx *next,
;                 struct mm *mm);
; ============================================================
ctx_switch:
    cli
    mov eax, [esp + 4]      ; prev
    mov edx, [esp + 8]      ; next
    mov ecx, [esp + 12]     ; mm

    ; PUSH registers onto prev's stack BEFORE saving ESP
    push ebx
    push esi
    push edi
    push ebp
    pushfd

    ; NOW save ESP (points to saved registers)
    mov [eax + OFF_K_ESP], esp

    ; Switch page directory
    mov ecx, [ecx]          ; mm->impl
    mov ecx, [ecx + 4]      ; mm_impl->pd_pa
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



; ============================================================
; void ctx_setup_fork_return(struct cpu_ctx *cpu_ctx);
; Sets up child's kernel stack to return to sys_return
; ============================================================
ctx_setup_fork_return:
    push ebp
    mov ebp, esp

    mov eax, [ebp+8]         ; cpu_ctx*
    mov edi, [eax + OFF_K_ESP]  ; Load kernel stack top

    ; Build kernel stack for ctx_switch to restore
    ; ctx_switch will: popfd, pop ebp, pop edi, pop esi, pop ebx, ret

    ; Return address - where ctx_switch will ret to
    sub edi, 4
    mov edx, sys_return
    mov [edi], edx

    ; Saved registers
    sub edi, 4
    mov dword [edi], 0       ; EBX

    sub edi, 4
    mov dword [edi], 0       ; ESI

    sub edi, 4
    mov dword [edi], 0       ; EDI

    sub edi, 4
    mov dword [edi], 0       ; EBP

    sub edi, 4
    mov dword [edi], 0x202   ; EFLAGS (IF=1)

    ; Save kernel ESP back to cpu_ctx
    mov [eax + OFF_K_ESP], edi

    pop ebp
    ret