#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>
#include "kutils.h"   // for k_strcmp

/* ------------------------------------------------------------
 * Minimal 32-bit ELF types (freestanding)
 * ------------------------------------------------------------ */

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint32_t      e_entry;    /* Entry point address */
    uint32_t      e_phoff;    /* Program header table file offset */
    uint32_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;    /* Number of program headers */
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type;   /* Segment type */
    uint32_t p_offset; /* Offset in file */
    uint32_t p_vaddr;  /* Virtual address in memory */
    uint32_t p_paddr;
    uint32_t p_filesz; /* Size of segment in file */
    uint32_t p_memsz;  /* Size of segment in memory */
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

#define PT_LOAD 1

/* ------------------------------------------------------------
 * ELF Loader
 * ------------------------------------------------------------ */

uint32_t elf_load(const void *image, size_t size, uint32_t load_base);

/* ------------------------------------------------------------
 * Embedded application table
 * ------------------------------------------------------------ */

struct embedded_app {
    const char *name;
    const unsigned char *start;
    const unsigned char *end;
};

/* Defined in elf.c */
extern const struct embedded_app embedded_apps[];
extern const size_t embedded_app_count;

const struct embedded_app *find_app(const char *name);

#endif // ELF_H
