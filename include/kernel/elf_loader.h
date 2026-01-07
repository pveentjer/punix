#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kutils.h"
#include "dirent.h"

/* ------------------------------------------------------------
 * Minimal 32-bit ELF types (freestanding)
 * ------------------------------------------------------------ */

#define EI_NIDENT 16
#define ET_EXEC 2

typedef struct
{
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;    /* entry virtual address within the image */
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct
{
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;   /* segment virtual address within the image */
    uint32_t p_paddr;   /* ignored */
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

typedef struct
{
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;      /* section virtual address within the image */
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

typedef struct
{
    uint32_t st_name;
    uint32_t st_value;  /* symbol virtual address within the image */
    uint32_t st_size;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
};

#define PT_LOAD 1

#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_HASH     5
#define SHT_DYNAMIC  6
#define SHT_NOTE     7
#define SHT_NOBITS   8
#define SHT_REL      9
#define SHT_SHLIB    10
#define SHT_DYNSYM   11

/*
 * elf_info contract:
 * - All values are virtual addresses.
 * - Offsets are relative to the provided load_base (i.e. within the process).
 */
struct elf_info
{
    uint32_t entry_va;        /* entry virtual address (load_base + e_entry) */
    uint32_t base_va;         /* the load_base passed to elf_load() */
    uint32_t max_offset;      /* highest (p_vaddr + p_memsz) within the image */
    uint32_t size;            /* total bytes copied from file (optional) */

    uint32_t environ_off;     /* offset of 'environ' from base_va (0 if absent) */
    uint32_t curbrk_off;      /* offset of '__curbrk' from base_va (0 if absent) */
};

/* ------------------------------------------------------------
 * ELF Loader
 * ------------------------------------------------------------ */

/*
 * Load ELF image into memory
 * Returns 0 on success, <0 on error.
 */
int elf_load(const void *image, struct elf_info *elf_info);

/* ------------------------------------------------------------
 * Embedded bin table
 * ------------------------------------------------------------ */

struct embedded_bin
{
    const char *name;
    const unsigned char *start;
    const unsigned char *end;
};

extern const struct embedded_bin embedded_bins[];
extern const size_t embedded_bin_count;

const struct embedded_bin *find_bin(const char *name);

#endif /* ELF_LOADER_H */
