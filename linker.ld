ENTRY(kmain)

/* Kernel linked to run at physical 1 MiB. */
SECTIONS
{
    . = 1M;                     /* 1 MiB = 0x00100000 */

    /* Small fixed header at the very start of the image */
    .kernel_header : ALIGN(4) {
        __kernel_header_base = .;

        /* 0x00100000: pointer to kernel entry (kmain) */
        LONG(kmain)

        /* 0x00100004: pointer to sys_enter_entry */
        LONG(sys_enter_entry)
    }

    /* Now put the actual code right after the header */
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
