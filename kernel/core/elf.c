#include "../../include/kernel/kutils.h"
#include "../../include/kernel/console.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/kutils.h"  // (you can drop this duplicate if you want)

/* ELF binary symbols from objcopy */
extern unsigned char _binary_swapper_elf_start[];
extern unsigned char _binary_swapper_elf_end[];
extern unsigned char _binary_init_elf_start[];
extern unsigned char _binary_init_elf_end[];
extern unsigned char _binary_sh_elf_start[];
extern unsigned char _binary_sh_elf_end[];
extern unsigned char _binary_loop_elf_start[];
extern unsigned char _binary_loop_elf_end[];
extern unsigned char _binary_ps_elf_start[];
extern unsigned char _binary_ps_elf_end[];
extern unsigned char _binary_test_args_elf_start[];
extern unsigned char _binary_test_args_elf_end[];
extern unsigned char _binary_spawn_chain_elf_start[];
extern unsigned char _binary_spawn_chain_elf_end[];
extern unsigned char _binary_kill_elf_start[];
extern unsigned char _binary_kill_elf_end[];
extern unsigned char _binary_ls_elf_start[];
extern unsigned char _binary_ls_elf_end[];
extern unsigned char _binary_cat_elf_start[];
extern unsigned char _binary_cat_elf_end[];
extern unsigned char _binary_echo_elf_start[];
extern unsigned char _binary_echo_elf_end[];
extern unsigned char _binary_print_env_elf_start[];
extern unsigned char _binary_print_env_elf_end[];
extern unsigned char _binary_tty_elf_start[];
extern unsigned char _binary_tty_elf_end[];

const struct embedded_app embedded_apps[] = {
        {"/sbin/swapper",    _binary_swapper_elf_start,     _binary_swapper_elf_end},
        {"/sbin/init",       _binary_init_elf_start,        _binary_init_elf_end},
        {"/bin/sh",          _binary_sh_elf_start,          _binary_sh_elf_end},
        {"/bin/loop",        _binary_loop_elf_start,        _binary_loop_elf_end},
        {"/bin/ps",          _binary_ps_elf_start,          _binary_ps_elf_end},
        {"/bin/test_args",   _binary_test_args_elf_start,   _binary_test_args_elf_end},
        {"/bin/spawn_chain", _binary_spawn_chain_elf_start, _binary_spawn_chain_elf_end},
        {"/bin/kill",        _binary_kill_elf_start,        _binary_kill_elf_end},
        {"/bin/ls",          _binary_ls_elf_start,          _binary_ls_elf_end},
        {"/bin/cat",         _binary_cat_elf_start,         _binary_cat_elf_end},
        {"/bin/echo",        _binary_echo_elf_start,        _binary_echo_elf_end},
        {"/bin/print_env",   _binary_print_env_elf_start,   _binary_print_env_elf_end},
        {"/bin/tty",         _binary_tty_elf_start,         _binary_tty_elf_end},
};

const size_t embedded_app_count =
        sizeof(embedded_apps) / sizeof(embedded_apps[0]);

const struct embedded_app *find_app(const char *name)
{
    for (size_t i = 0; i < embedded_app_count; i++)
    {
        if (k_strcmp(embedded_apps[i].name, name) == 0)
            return &embedded_apps[i];
    }
    return NULL;
}

/* ------------------------------------------------------------
 * Fill dirent entries for /bin from embedded_apps
 * ------------------------------------------------------------ */

static void fill_dirent_from_app(struct dirent *de,
                                 const struct embedded_app *app,
                                 uint32_t ino)
{
    de->d_ino = ino;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type = DT_REG;

    /* Strip any leading path: "/bin/ls" -> "ls" */
    const char *name = app->name;
    const char *base = name;

    const char *p = k_strrchr(name, '/');
    if (p && p[1] != '\0')
        base = p + 1;

    size_t len = k_strlen(base);
    if (len >= sizeof(de->d_name))
        len = sizeof(de->d_name) - 1;

    k_memcpy(de->d_name, base, len);
    de->d_name[len] = '\0';
}

int elf_fill_bin_dirents(struct dirent *buf, unsigned int max_entries)
{
    unsigned int idx = 0;

    if (!buf || max_entries == 0)
        return 0;

    for (size_t i = 0; i < embedded_app_count && idx < max_entries; ++i)
    {
        fill_dirent_from_app(&buf[idx], &embedded_apps[i],
                             (uint32_t) (i + 1));  // fake inode
        idx++;
    }

    return (int) idx;
}

/* --------------------------------------------------------------------
 * ELF loader with relocation fixups and symbol table parsing
 * -------------------------------------------------------------------- */

int elf_load(const void *image, size_t size, uint32_t load_base, struct elf_info *out)
{
    (void) size;

    const Elf32_Ehdr *eh = (const Elf32_Ehdr *) image;

    // Basic ELF magic check
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
    {
        kprintf("Invalid ELF magic\n");
        return -1;
    }

    const Elf32_Phdr *ph =
            (const Elf32_Phdr *) ((uintptr_t) image + eh->e_phoff);

    uint32_t max_end = 0;
    uint32_t total_copied = 0;

    // Copy all PT_LOAD segments to load_base + p_vaddr
    for (int i = 0; i < eh->e_phnum; i++, ph++)
    {
        if (ph->p_type != PT_LOAD)
            continue;

        uint32_t dest_addr = load_base + ph->p_vaddr;
        void *dest = (void *) dest_addr;
        const void *src = (const void *) ((uintptr_t) image + ph->p_offset);

        k_memcpy(dest, src, ph->p_filesz);
        total_copied += ph->p_filesz;

        if (ph->p_memsz > ph->p_filesz)
        {
            k_memset((uint8_t *) dest + ph->p_filesz, 0,
                     ph->p_memsz - ph->p_filesz);
        }

        uint32_t seg_end = ph->p_vaddr + ph->p_memsz;
        if (seg_end > max_end)
            max_end = seg_end;
    }

    if (out)
    {
        out->base = load_base;
        out->entry = load_base + eh->e_entry;
        out->max_offset = max_end;       // still in "process space"
        out->size = total_copied;
        out->environ_addr = 0;  // default to 0 if not found
    }

    // Parse symbol table to find 'environ'
    if (eh->e_shoff != 0 && eh->e_shnum > 0 && out)
    {
        const Elf32_Shdr *sh = (const Elf32_Shdr *) ((uintptr_t) image + eh->e_shoff);

        const Elf32_Shdr *symtab = NULL;
        const Elf32_Shdr *strtab = NULL;

        // Find symbol table and its associated string table
        for (int i = 0; i < eh->e_shnum; i++)
        {
            if (sh[i].sh_type == SHT_SYMTAB)
            {
                symtab = &sh[i];
                // sh_link points to the string table for this symbol table
                if (sh[i].sh_link < eh->e_shnum)
                {
                    strtab = &sh[sh[i].sh_link];
                }
                break;
            }
        }

        // If we found both symbol table and string table, search for 'environ' and '__curbrk'
        if (symtab && strtab && symtab->sh_entsize >= sizeof(Elf32_Sym))
        {
            const Elf32_Sym *symbols =
                    (const Elf32_Sym *) ((uintptr_t) image + symtab->sh_offset);
            const char *strings =
                    (const char *) ((uintptr_t) image + strtab->sh_offset);

            uint32_t sym_count = symtab->sh_size / symtab->sh_entsize;

            for (uint32_t i = 0; i < sym_count; i++)
            {
                // Check if symbol name index is valid
                if (symbols[i].st_name < strtab->sh_size)
                {
                    const char *sym_name = strings + symbols[i].st_name;

                    if (k_strcmp(sym_name, "environ") == 0)
                    {
                        out->environ_addr = symbols[i].st_value;
                    }
                    else if (k_strcmp(sym_name, "__curbrk") == 0)
                    {
                        out->curbrk_addr = symbols[i].st_value;
                    }
                }
            }
        }
    }

    return 0;
}