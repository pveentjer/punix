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

typedef struct {
    unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint32_t      e_entry;    /* Entry point address */
    uint32_t      e_phoff;    /* Program header table file offset */
    uint32_t      e_shoff;    /* Section header table file offset */
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;    /* Number of program headers */
    uint16_t      e_shentsize;
    uint16_t      e_shnum;    /* Number of section headers */
    uint16_t      e_shstrndx; /* Section header string table index */
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

typedef struct {
    uint32_t sh_name;      /* Section name (string table index) */
    uint32_t sh_type;      /* Section type */
    uint32_t sh_flags;     /* Section flags */
    uint32_t sh_addr;      /* Section virtual address at execution */
    uint32_t sh_offset;    /* Section file offset */
    uint32_t sh_size;      /* Section size in bytes */
    uint32_t sh_link;      /* Link to another section */
    uint32_t sh_info;      /* Additional section information */
    uint32_t sh_addralign; /* Section alignment */
    uint32_t sh_entsize;   /* Entry size if section holds table */
} Elf32_Shdr;

typedef struct {
    uint32_t st_name;   /* Symbol name (string table index) */
    uint32_t st_value;  /* Symbol value (address) */
    uint32_t st_size;   /* Symbol size */
    uint8_t  st_info;   /* Symbol type and binding */
    uint8_t  st_other;  /* Symbol visibility */
    uint16_t st_shndx;  /* Section index */
} Elf32_Sym;

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

struct elf_info {
    uint32_t entry;          // entry point (absolute address)
    uint32_t base;           // process base address (load_base)
    uint32_t max_offset;     // highest p_vaddr + p_memsz in ELF
    uint32_t size;           // total bytes copied (optional)
    uint32_t environ_addr;   // address of environ variable (0 if not found)
    uint32_t curbrk_addr;    // address of __curbrk variable (0 if not found)
};


/* ------------------------------------------------------------
 * ELF Loader
 * ------------------------------------------------------------ */

int elf_load(const void *image, size_t size,
             uint32_t load_base, struct elf_info *out);

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

int elf_fill_bin_dirents(struct dirent *buf, unsigned int max_entries);

#endif // ELF_LOADER_H