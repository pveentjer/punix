OUTPUT_FORMAT(elf32-i386)
ENTRY(main)

PHDRS
{
    text PT_LOAD FLAGS(5);  /* R-X */
    data PT_LOAD FLAGS(6);  /* RW- */
}

SECTIONS
{
    /* User space starts at 4MB */
    . = 0x00400000;

    .text : ALIGN(4096) {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    } :text

    .data : ALIGN(4096) {
        *(.data .data.*)
    } :data

    .bss : ALIGN(4096) {
        *(.bss .bss.* COMMON)
    } :data
}
