ENTRY(kmain)

/* Kernel linked to run at physical 1 MiB, virtual addresses start at 2GB */
SECTIONS
{
    . = 0x80000000;            /* virtual base at 2GB (higher half) */

    /* Small fixed header at the very start of the image */
    .kernel_header : ALIGN(4) {
        __kernel_header_base = .;

        /* pointer to kernel entry (kpremain in kpremain.asm) */
        LONG(_kpremain)

        /* pointer to sys_enter */
        LONG(sys_enter)
    }

    .text : ALIGN(4) {
        *(.start)
        *(.text*)
    }

    .rodata : ALIGN(4) {
        *(.rodata*)
        KEEP(*(.sys_calls))
    }

    .data : ALIGN(4) {
        *(.data*)
    }

    .bss (NOLOAD) : ALIGN(16)
    {
       __bss_start = .;
       *(.bss*)
       *(COMMON)
       __bss_end = .;
    }

    _kernel_end = .;
}