ENTRY(kmain)

SECTIONS
{
    . = 0x00100000;

    .kernel_header : ALIGN(4) {
        __kernel_api_ptr = .;
        LONG(kernel_api_instance)    /* <-- directly use the symbol */
    }

    .text : ALIGN(4) {
        *(.start)
        *(.text*)
    }

    .rodata : ALIGN(4) {
        *(.rodata*)
        KEEP(*(.kernel_api))         /* contains kernel_api_instance */
    }

    .data : ALIGN(4) {
        *(.data*)
    }

    .bss : ALIGN(4) {
        *(.bss*)
        *(COMMON)
    }
}
