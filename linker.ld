ENTRY(kmain)

SECTIONS
{
    /* Kernel load address */
    . = 0x10000;

    /* --- KERNEL HEADER (fixed location) --- */
    .kernel_header : ALIGN(4) {
        /* This address (0x10000) is stable */
        __kernel_api_ptr = .;
        /* Linker writes the address of kernel_api_instance here */
        LONG(__kernel_api)
    }

    /* --- CODE SECTION --- */
    .text : ALIGN(4) {
        *(.start)      /* kmain and other early entry code first */
        *(.text*)      /* then everything else */
    }

    /* --- READ-ONLY DATA (consts, strings, kernel_api, etc.) --- */
    .rodata : ALIGN(4) {
        *(.rodata*)

        /* Kernel API jump table lives here */
        __kernel_api = .;
        KEEP(*(.kernel_api))
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
