ENTRY(kmain)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.start)   /* put kmain section first */
        *(.text*)   /* then the rest of the code */
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }
}
