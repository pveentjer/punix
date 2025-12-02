ENTRY(kmain)

SECTIONS
{
    /* Kernel load address */
    . = 0x10000;

    /* --- CODE SECTION --- */
    .text : ALIGN(4) {
        *(.start)      /* kmain and other early entry code first */
        *(.text*)      /* then everything else */
    }

    /* --- READ-ONLY DATA (consts, strings, etc.) --- */
    .rodata : ALIGN(4) {
        *(.rodata*)
    }

    /* --- PROCESS REGISTRATION TABLE --- */
    .proctable : ALIGN(4) {
        __proctable_start = .;
        KEEP(*(.proctable))
        __proctable_end = .;
    }

    /* --- INITIALIZED DATA --- */
    .data : ALIGN(4) {
        *(.data*)
    }

    /* --- ZERO-INITIALIZED DATA --- */
    .bss : ALIGN(4) {
        *(.bss*)
        *(COMMON)
    }
}
