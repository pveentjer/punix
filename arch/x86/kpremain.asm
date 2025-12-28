; ============================================================
; arch/x86/kpremain.asm
; ============================================================
BITS 32
global _kpremain
extern kmain

extern __kernel_va_base
extern __kernel_pa_start
extern __kernel_low_page_table_va
extern __kernel_high_page_table_va
extern __kernel_page_directory_va

; ------------------------------------------------------------
; Basic macros
; ------------------------------------------------------------
%define KB(x) ((x) << 10)
%define MB(x) ((x) << 20)
%define GB(x) ((x) << 30)

%define PAGE_SIZE           KB(4)
%define PAGE_SHIFT          12
%define PAGE_MASK           (~(PAGE_SIZE - 1))

%define KERNEL_VA_BASE      GB(2)
%define KERNEL_VA_SIZE      MB(4)
%define KERNEL_VA_STACK_TOP (KERNEL_VA_BASE + KERNEL_VA_SIZE - PAGE_SIZE)

%define KERNEL_LOAD_PA      MB(1)
%define PREMAIN_PA          KERNEL_LOAD_PA

; ------------------------------------------------------------
; Low memory allocations
; ------------------------------------------------------------
%define VGA_PA              0x000B8000
%define VGA_VA              VGA_PA
%define VGA_SIZE            PAGE_SIZE
%define VGA_END_PA          (VGA_PA + VGA_SIZE)

; Place GDT immediately after VGA, on a page boundary
%define GDT_PA              (VGA_END_PA)
%define GDTR_PA             (GDT_PA + GDT_TOTAL_BYTES)

; Minimal early stack (must be identity-mapped; keep it inside first 4MB)
%define EARLY_STACK_TOP_PA  (MB(1) - KB(16))     ; 1MB - 16KB

; ------------------------------------------------------------
; GDT layout / selectors
; ------------------------------------------------------------
%define GDT_ENTRY_BYTES     8
%define GDT_NENTRIES        3

%define GDT_TOTAL_BYTES     (GDT_NENTRIES * GDT_ENTRY_BYTES)
%define GDT_LIMIT_BYTES     (GDT_TOTAL_BYTES - 1)

%define GDT_NULL_INDEX      0
%define GDT_CODE_INDEX      1
%define GDT_DATA_INDEX      2

%define GDT_SEL(idx)        ((idx) * GDT_ENTRY_BYTES)

%define GDT_CODE            GDT_SEL(GDT_CODE_INDEX)
%define GDT_DATA            GDT_SEL(GDT_DATA_INDEX)
%define KERNEL_DS           GDT_DATA

%define GDT_OFF_NULL        (GDT_SEL(GDT_NULL_INDEX))
%define GDT_OFF_CODE        (GDT_SEL(GDT_CODE_INDEX))
%define GDT_OFF_DATA        (GDT_SEL(GDT_DATA_INDEX))

; ------------------------------------------------------------
; Paging
; ------------------------------------------------------------
%define PTE_PRESENT         1
%define PTE_RW              2
%define PTE_FLAGS           (PTE_PRESENT | PTE_RW)

%define PTE_BITS            10
%define PDE_SHIFT           (PAGE_SHIFT + PTE_BITS)
%define PDE_INDEX(x)        ((x) >> PDE_SHIFT)

%define PG_BIT              31

; ------------------------------------------------------------
; GDT encoding constants (flat 0..4GB, 32-bit, 4K gran)
; ------------------------------------------------------------
%define GDT_ACC_CODE        0x9A
%define GDT_ACC_DATA        0x92

%define GDT_GRAN_4K         0x80
%define GDT_GRAN_32BIT      0x40
%define GDT_GRAN_LIMIT_HI   0x0F
%define GDT_FLAGS           (GDT_GRAN_4K | GDT_GRAN_32BIT | GDT_GRAN_LIMIT_HI)

%define GDT_BASE            0x00000000
%define GDT_LIMIT           0x000FFFFF            ; 4GB when granularity=4K

; Build the descriptor dwords without magic constants
%define GDT_LODWORD(base, limit) ( ((limit) & 0xFFFF) | (((base) & 0xFFFF) << 16) )
%define GDT_HIDWORD(base, access, flags, limit) ( (((base) >> 16) & 0xFF) | (((access) & 0xFF) << 8) | \
                                                  ((((limit) >> 16) & 0x0F) << 16) | (((flags) & 0xF0) << 16) | \
                                                  (((base) >> 24) << 24) )

; ============================================================
; Entry
; ============================================================
section .text.premain

section .text.premain

_kpremain:
    cli

    ; Flat segments
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Temporary stack in identity-mapped low memory
    mov esp, 589824

    ; build GDT
    call build_min_gdt_low


    ; Zero PD + both PTs
    call clear_paging_structs

    ; Fill low PT: identity map premain/VGA only
    ; (kernel header is no longer identity-mapped)
    call map_identity_premain
    call map_identity_vga
    call map_identity_gdt

    ; Fill high PT: map 1MB window at KERNEL_VA_BASE -> __kernel_pa_start
    ; This now includes the kernel header as the first page
    call map_kernel_high

    ; Install PDE[0] = low PT (identity mappings)
    call get_low_pt_pa
    mov ebx, eax
    mov eax, 0
    call install_pt_at_pde_pa

    ; Install PDE[KERNEL_VA_BASE>>22] = high PT (high kernel window)
    call get_high_pt_pa
    mov ebx, eax
    mov eax, PDE_INDEX(KERNEL_VA_BASE)
    call install_pt_at_pde_pa

    ; Load page directory base
    call get_pd_pa
    mov cr3, eax

    ; Enable paging
    mov eax, cr0
    or  eax, (1 << PG_BIT)
    mov cr0, eax

    ; load GDT
    lgdt [GDTR_PA] ; since it is identity mapped, we can use the physical address.

    ; Far jump to reload CS with new GDT
    jmp GDT_CODE:.reload_cs

.reload_cs:
    ; Now CS is loaded from new GDT

    ; Reload data segments too
    mov ax, GDT_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Switch to high virtual stack
    mov esp, KERNEL_VA_STACK_TOP
    jmp kmain

; ============================================================
; GDT construction at fixed low PA (VA==PA at this stage)
; ============================================================
build_min_gdt_low:
    ; null descriptor
    mov dword [GDT_PA + GDT_OFF_NULL + 0], 0
    mov dword [GDT_PA + GDT_OFF_NULL + 4], 0

    ; code descriptor
    mov dword [GDT_PA + GDT_OFF_CODE + 0], GDT_LODWORD(GDT_BASE, GDT_LIMIT)
    mov dword [GDT_PA + GDT_OFF_CODE + 4], GDT_HIDWORD(GDT_BASE, GDT_ACC_CODE, GDT_FLAGS, GDT_LIMIT)

    ; data descriptor
    mov dword [GDT_PA + GDT_OFF_DATA + 0], GDT_LODWORD(GDT_BASE, GDT_LIMIT)
    mov dword [GDT_PA + GDT_OFF_DATA + 4], GDT_HIDWORD(GDT_BASE, GDT_ACC_DATA, GDT_FLAGS, GDT_LIMIT)

    ; GDTR (limit:16, base:32)
    mov word  [GDTR_PA + 0], GDT_LIMIT_BYTES
    mov dword [GDTR_PA + 2], GDT_PA
    ret

; ============================================================
; VA -> PA helper for linker symbols in high kernel window
; Input : eax = VA
; Output: eax = PA
; ============================================================
va_to_pa:
    push ebx
    mov ebx, __kernel_va_base
    sub eax, ebx
    mov ebx, __kernel_pa_start
    add eax, ebx
    pop ebx
    ret

; ============================================================
; Paging structures physical addresses (from linker VAs)
; ============================================================
get_low_pt_pa:
    mov eax, __kernel_low_page_table_va
    call va_to_pa
    ret

get_high_pt_pa:
    mov eax, __kernel_high_page_table_va
    call va_to_pa
    ret

get_pd_pa:
    mov eax, __kernel_page_directory_va
    call va_to_pa
    ret

; ============================================================
; Clear paging structures: low PT, high PT, PD.
; Clobbers: eax, ecx, edi
; ============================================================
clear_paging_structs:
    xor eax, eax

    call get_low_pt_pa
    mov edi, eax
    mov ecx, PAGE_SIZE / 4
    rep stosd

    call get_high_pt_pa
    mov edi, eax
    mov ecx, PAGE_SIZE / 4
    rep stosd

    call get_pd_pa
    mov edi, eax
    mov ecx, PAGE_SIZE / 4
    rep stosd
    ret

; ============================================================
; Install a PT into PD at PDE index.
; Input: eax = pde_index, ebx = pt_pa
; ============================================================
install_pt_at_pde_pa:
    push ecx
    push edi

    mov ecx, eax
    call get_pd_pa
    mov edi, eax

    mov eax, ebx
    or  eax, PTE_FLAGS

    shl ecx, 2
    mov [edi + ecx], eax

    pop edi
    pop ecx
    ret

; ============================================================
; Write a PTE into a given PT.
; Input: eax = VA, ebx = PA, ecx = flags, edi = pt_base_pa
; ============================================================
map_page_pt:
    mov edx, eax
    shr edx, PAGE_SHIFT
    and edx, 0x3FF
    shl edx, 2

    mov eax, ebx
    or  eax, ecx
    mov [edi + edx], eax
    ret

; ============================================================
; Identity mappings (low PT)
; ============================================================
map_identity_premain:
    call get_low_pt_pa
    mov edi, eax
    mov eax, PREMAIN_PA
    mov ebx, PREMAIN_PA
    mov ecx, PTE_FLAGS
    call map_page_pt
    ret

map_identity_vga:
    call get_low_pt_pa
    mov edi, eax
    mov eax, VGA_VA
    mov ebx, VGA_PA
    mov ecx, PTE_FLAGS
    call map_page_pt
    ret

; Identity-map the page(s) covering GDT + GDTR
map_identity_gdt:
    call get_low_pt_pa
    mov edi, eax
    mov ecx, PTE_FLAGS

    mov eax, (GDT_PA & PAGE_MASK)
    mov ebx, eax
    call map_page_pt

    mov eax, (GDTR_PA & PAGE_MASK)
    cmp eax, (GDT_PA & PAGE_MASK)
    je .done

    mov ebx, eax
    call map_page_pt

.done:
    ret

; ============================================================
; Map KERNEL_VA_SIZE bytes at:
;   VA: KERNEL_VA_BASE .. KERNEL_VA_BASE+KERNEL_VA_SIZE
;   PA: __kernel_pa_start .. +KERNEL_VA_SIZE
; Uses the high PT.
; ============================================================
map_kernel_high:
    call get_high_pt_pa
    mov edi, eax

    mov esi, KERNEL_VA_BASE
    mov ebx, __kernel_pa_start
    mov ecx, PTE_FLAGS

    mov ebp, KERNEL_VA_SIZE
    shr ebp, PAGE_SHIFT

.loop:
    mov eax, esi
    call map_page_pt

    add esi, PAGE_SIZE
    add ebx, PAGE_SIZE
    dec ebp
    jnz .loop
    ret
