OUTPUT_FORMAT(elf32-i386)
ENTRY(_kpremain)

KERNEL_LOAD_PA = 1048576;
KERNEL_VMA     = 2148532224;
PAGE_SIZE      = 4096;

KERNEL_HDR_PA  = KERNEL_LOAD_PA + 0 * PAGE_SIZE;
PREMAIN_PA     = KERNEL_LOAD_PA + 1 * PAGE_SIZE;

SECTIONS
{
  /* ------------------------------------------------------------
   * Identity-mapped header page
   * ------------------------------------------------------------ */
  . = KERNEL_HDR_PA;
  .kernel_header : AT(KERNEL_HDR_PA) ALIGN(PAGE_SIZE)
  {
    FILL(0)

    __kernel_header_base = .;

    LONG(_kpremain)                   /* +0 */
    LONG(sys_enter)                   /* +4 */
    LONG(__kernel_page_directory_va)  /* +8 */

    . = ALIGN(PAGE_SIZE);
    __kernel_hdr_end_pa = .;
  }

  /* ------------------------------------------------------------
   * Identity-mapped premain page(s)
   * ------------------------------------------------------------ */
  . = PREMAIN_PA;
  .premain : AT(PREMAIN_PA) ALIGN(PAGE_SIZE)
  {
    __premain_pa_start = LOADADDR(.premain);
    __premain_va_start = ADDR(.premain);

    KEEP(*(.text.premain))
    KEEP(*(.rodata.premain))
    KEEP(*(.data.premain))

    . = ALIGN(PAGE_SIZE);

    __premain_pa_end = LOADADDR(.premain) + SIZEOF(.premain);
    __premain_va_end = ADDR(.premain) + SIZEOF(.premain);
  }

  /* ------------------------------------------------------------
   * High kernel VMA, packed LMA after premain
   * ------------------------------------------------------------ */
  . = KERNEL_VMA;
  __kernel_va_start = .;

  __phys = __premain_pa_end;
  __kernel_pa_start = __phys;

  .text ALIGN(PAGE_SIZE) : AT(__phys)
  {
    __text_start = .;
    *(.text .text.*)
    *(.start .start.*)
    __text_end = .;
  }
  __phys = ALIGN(__phys + SIZEOF(.text), PAGE_SIZE);

  .rodata ALIGN(PAGE_SIZE) : AT(__phys)
  {
    __rodata_start = .;
    *(.rodata .rodata.*)
    __rodata_end = .;
  }
  __phys = ALIGN(__phys + SIZEOF(.rodata), PAGE_SIZE);

  .data ALIGN(PAGE_SIZE) : AT(__phys)
  {
    __data_start = .;
    *(.data .data.*)
    __data_end = .;
  }
  __phys = ALIGN(__phys + SIZEOF(.data), PAGE_SIZE);

  /* ------------------------------------------------------------
   * Kernel paging structures (physically present)
   * ------------------------------------------------------------ */
  .kernel_pages ALIGN(PAGE_SIZE) : AT(__phys)
  {
    FILL(0)

    __kernel_page_table_va = .;
    LONG(0)                       /* force PROGBITS/LOAD */
    . = __kernel_page_table_va + PAGE_SIZE;

    __kernel_page_directory_va = .;
    . = __kernel_page_directory_va + PAGE_SIZE;
  }
  __phys = ALIGN(__phys + SIZEOF(.kernel_pages), PAGE_SIZE);

  /* ------------------------------------------------------------
   * BSS (NOLOAD)
   * ------------------------------------------------------------ */
  .bss ALIGN(PAGE_SIZE) (NOLOAD) :
  {
    __bss_start = .;
    *(.bss .bss.* COMMON)
    __bss_end = .;
  }

  __kernel_va_end = .;
  __kernel_pa_end = __phys;

  /* ------------------------------------------------------------
   * HARD DISCARD: no junk in the image, ever
   * ------------------------------------------------------------ */
  /DISCARD/ :
  {
    *(.eh_frame .eh_frame.*)
    *(.note.gnu.build-id)
    *(.note.*)
    *(.comment)
  }
}
