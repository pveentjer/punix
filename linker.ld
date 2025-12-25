OUTPUT_FORMAT(elf32-i386)
ENTRY(_kpremain)

KERNEL_LOAD_PA = 1048576;
KERNEL_VMA     = 2148532224;

PAGE_SIZE      = 4096;

KERNEL_HDR_PA  = KERNEL_LOAD_PA + 0 * PAGE_SIZE;
PREMAIN_PA     = KERNEL_LOAD_PA + 1 * PAGE_SIZE;
EARLY_PA       = KERNEL_LOAD_PA + 2 * PAGE_SIZE;

BOOT_LIMIT     = 65536;

SECTIONS
{
  . = KERNEL_HDR_PA;
  .kernel_header KERNEL_HDR_PA : AT(KERNEL_HDR_PA) ALIGN(PAGE_SIZE)
  {
    __kernel_header_base = .;

    LONG(_kpremain)   /* +0 */
    LONG(sys_enter)   /* +4 */

    . = ALIGN(PAGE_SIZE);
    __kernel_hdr_end_pa = .;
  }

  . = PREMAIN_PA;
  .premain PREMAIN_PA : AT(PREMAIN_PA) ALIGN(PAGE_SIZE)
  {
    __premain_pa_start = LOADADDR(.premain);
    __premain_va_start = ADDR(.premain);

    KEEP(*(.text.premain))
    KEEP(*(.rodata.premain))
    KEEP(*(.data.premain))
    KEEP(*(.early))

    . = ALIGN(PAGE_SIZE);
    __premain_pa_end = LOADADDR(.premain) + SIZEOF(.premain);
    __premain_va_end = ADDR(.premain) + SIZEOF(.premain);
  }

  . = KERNEL_VMA;
  __kernel_va_start = .;
  __kernel_pa_start = __premain_pa_end;

  .text ALIGN(PAGE_SIZE) : AT(__kernel_pa_start)
  {
    __text_start = .;
    *(.text .text.*)
    __text_end = .;
  }

  .rodata ALIGN(PAGE_SIZE)
    : AT(__kernel_pa_start + (__rodata_start - __kernel_va_start))
  {
    __rodata_start = .;
    *(.rodata .rodata.*)
    __rodata_end = .;
  }

  .data ALIGN(PAGE_SIZE)
    : AT(__kernel_pa_start + (__data_start - __kernel_va_start))
  {
    __data_start = .;
    *(.data .data.*)
    __data_end = .;
  }

  .bss ALIGN(PAGE_SIZE) (NOLOAD):
  {
    __bss_start = .;
    *(.bss .bss.* COMMON)
    __bss_end = .;
  }

  __kernel_va_end = .;
  __kernel_pa_end =
    __kernel_pa_start + (__kernel_va_end - __kernel_va_start);
}
