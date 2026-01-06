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

%define OFF_U_ESP  0
%define OFF_K_ESP  4

%define O_RDONLY   0
%define O_WRONLY   1

; Linux i386 auxv types we provide minimally
%define AT_NULL    0
%define AT_PAGESZ  6
%define AT_UID     11
%define AT_EUID    12
%define AT_GID     13
%define AT_EGID    14
%define AT_SECURE  23

; struct trampoline layout (32-bit)
%define TR_ENTRY     0    ; uint32_t main_addr
%define TR_ARGC      4    ; int
%define TR_ARGV      8    ; char **
%define TR_ENVP      12   ; char **

; ============================================================
; void ctx_setup_trampoline(struct cpu_ctx *cpu_ctx,
;                           const struct trampoline *tr);
; ============================================================
ctx_setup_trampoline:
    push ebp
    mov  ebp, esp

    ; [ebp+8]  = cpu_ctx
    ; [ebp+12] = trampoline*

    mov esi, [ebp+8]              ; esi = cpu_ctx*
    mov edi, [esi + OFF_K_ESP]    ; edi = kernel stack top

    mov eax, [ebp+12]             ; eax = trampoline*

    ; Push args for task_trampoline(entry, argc, argv, envp) (cdecl, reverse)

    sub edi, 4
    mov edx, [eax + TR_ENVP]
    mov [edi], edx                ; envp

    sub edi, 4
    mov edx, [eax + TR_ARGV]
    mov [edi], edx                ; argv

    sub edi, 4
    mov edx, [eax + TR_ARGC]
    mov [edi], edx                ; argc

    sub edi, 4
    mov edx, [eax + TR_ENTRY]
    mov [edi], edx                ; entry

    ; Dummy return address for task_trampoline's frame
    sub edi, 4
    mov dword [edi], 0

    ; Return address where ctx_switch will ret to
    sub edi, 4
    mov edx, task_trampoline
    mov [edi], edx

    ; CPU state that ctx_switch will restore (must match ctx_switch pop order)
    sub edi, 4
    mov dword [edi], 0x202        ; EFLAGS (IF=1)

    sub edi, 4
    mov dword [edi], 0            ; EBP

    sub edi, 4
    mov dword [edi], 0            ; EDI

    sub edi, 4
    mov dword [edi], 0            ; ESI

    sub edi, 4
    mov dword [edi], 0            ; EBX

    ; Save kernel ESP back to cpu_ctx
    mov [esi + OFF_K_ESP], edi

    pop ebp
    ret

; ============================================================
; void task_trampoline(void *entry, int argc, char **argv, char **envp)
;
; Builds Linux-style _start stack on the user stack:
;   argc, argv..., NULL, envp..., NULL, auxv..., AT_NULL
;
; Then jumps to entry (expected to be _start).
; ============================================================
task_trampoline:
    push ebp
    mov  ebp, esp
    sub  esp, 20                  ; locals
    push ebx
    push esi
    push edi

    ; Save args to locals
    mov eax, [ebp + 8]            ; entry
    mov [ebp - 4], eax
    mov eax, [ebp + 12]           ; argc
    mov [ebp - 8], eax
    mov eax, [ebp + 16]           ; argv
    mov [ebp - 12], eax
    mov eax, [ebp + 20]           ; envp
    mov [ebp - 16], eax

    ; current task
    call sched_current
    mov [ebp - 20], eax           ; current

    ; --- open stdin/stdout/stderr on kernel stack ---
    mov eax, [ebp - 20]

    push dword 0
    push dword O_RDONLY
    push stdin_str
    push eax
    call vfs_open
    add esp, 16

    mov eax, [ebp - 20]

    push dword 0
    push dword O_WRONLY
    push stdout_str
    push eax
    call vfs_open
    add esp, 16

    mov eax, [ebp - 20]

    push dword 0
    push dword O_WRONLY
    push stderr_str
    push eax
    call vfs_open
    add esp, 16

    ; --- switch to user stack ---
    mov edi, [ebp - 20]           ; current
    mov ecx, [edi + OFF_U_ESP]    ; user stack top

    ; Save kernel ESP
    mov ebx, esp
    mov [edi + OFF_K_ESP], ebx

    mov esp, ecx
    and esp, 0xFFFFFFF0           ; align down

    ; Load saved args into regs
    mov eax, [ebp - 4]            ; entry
    mov ebx, [ebp - 8]            ; argc
    mov edx, [ebp - 12]           ; argv
    mov esi, [ebp - 16]           ; envp

    ; ============================================================
    ; Push auxv (reverse order so auxv[0] ends up first in memory)
    ; ============================================================

    ; AT_NULL, 0
    push dword 0
    push dword AT_NULL

    ; AT_SECURE = 0
    push dword 0
    push dword AT_SECURE

    ; AT_EGID = 0
    push dword 0
    push dword AT_EGID

    ; AT_GID = 0
    push dword 0
    push dword AT_GID

    ; AT_EUID = 0
    push dword 0
    push dword AT_EUID

    ; AT_UID = 0
    push dword 0
    push dword AT_UID

    ; AT_PAGESZ = 4096
    push dword 4096
    push dword AT_PAGESZ

    ; ============================================================
    ; Push envp[] pointers + NULL terminator
    ; ============================================================
    xor ecx, ecx
.count_envp:
    cmp dword [esi + ecx*4], 0
    je  .push_envp
    inc ecx
    jmp .count_envp

.push_envp:
    push dword 0
.push_envp_loop:
    test ecx, ecx
    jz   .envp_done
    dec  ecx
    push dword [esi + ecx*4]
    jmp  .push_envp_loop
.envp_done:

    ; ============================================================
    ; Push argv[] pointers + NULL terminator
    ; ============================================================
    push dword 0
    mov ecx, ebx
.push_argv_loop:
    test ecx, ecx
    jz   .argv_done
    dec  ecx
    push dword [edx + ecx*4]
    jmp  .push_argv_loop
.argv_done:

    ; Push argc
    push ebx

    ; Jump to entry (_start)
    jmp eax

section .rodata
stdin_str:  db "/dev/stdin", 0
stdout_str: db "/dev/stdout", 0
stderr_str: db "/dev/stderr", 0

section .text

; ============================================================
; void ctx_switch(struct cpu_ctx *prev,
;                 struct cpu_ctx *next,
;                 struct mm *mm);
; ============================================================
ctx_switch:
    cli
    mov eax, [esp + 4]            ; prev
    mov edx, [esp + 8]            ; next
    mov ecx, [esp + 12]           ; mm

    push ebx
    push esi
    push edi
    push ebp
    pushfd

    mov [eax + OFF_K_ESP], esp

    mov ecx, [ecx]
    mov ecx, [ecx + 4]
    mov cr3, ecx

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
; ============================================================
ctx_setup_fork_return:
    push ebp
    mov  ebp, esp

    mov eax, [ebp+8]
    mov edi, [eax + OFF_K_ESP]

    sub edi, 4
    mov edx, sys_return
    mov [edi], edx

    sub edi, 4
    mov dword [edi], 0            ; EBX
    sub edi, 4
    mov dword [edi], 0            ; ESI
    sub edi, 4
    mov dword [edi], 0            ; EDI
    sub edi, 4
    mov dword [edi], 0            ; EBP
    sub edi, 4
    mov dword [edi], 0x202        ; EFLAGS

    mov [eax + OFF_K_ESP], edi

    pop ebp
    ret
