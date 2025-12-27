#include "kernel/kutils.h"
#include "kernel/console.h"
#include "kernel/elf_loader.h"

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
extern unsigned char _binary_pwd_elf_start[];
extern unsigned char _binary_pwd_elf_end[];
extern unsigned char _binary_date_elf_start[];
extern unsigned char _binary_date_elf_end[];
extern unsigned char _binary_uptime_elf_start[];
extern unsigned char _binary_uptime_elf_end[];
extern unsigned char _binary_clear_elf_start[];
extern unsigned char _binary_clear_elf_end[];
extern unsigned char _binary_time_elf_start[];
extern unsigned char _binary_time_elf_end[];

const struct embedded_app embedded_apps[] = {
        {"/sbin/swapper",    _binary_swapper_elf_start,     _binary_swapper_elf_end},
        {"/sbin/init",       _binary_init_elf_start,        _binary_init_elf_end},
        {"/bin/sh",          _binary_sh_elf_start,          _binary_sh_elf_end},
        {"/bin/loop",        _binary_loop_elf_start,        _binary_loop_elf_end},
        {"/bin/ps",          _binary_ps_elf_start,          _binary_ps_elf_end},
        {"/bin/spawn_chain", _binary_spawn_chain_elf_start, _binary_spawn_chain_elf_end},
        {"/bin/kill",        _binary_kill_elf_start,        _binary_kill_elf_end},
        {"/bin/ls",          _binary_ls_elf_start,          _binary_ls_elf_end},
        {"/bin/cat",         _binary_cat_elf_start,         _binary_cat_elf_end},
        {"/bin/echo",        _binary_echo_elf_start,        _binary_echo_elf_end},
        {"/bin/print_env",   _binary_print_env_elf_start,   _binary_print_env_elf_end},
        {"/bin/tty",         _binary_tty_elf_start,         _binary_tty_elf_end},
        {"/bin/pwd",         _binary_pwd_elf_start,         _binary_pwd_elf_end},
        {"/bin/date",        _binary_date_elf_start,        _binary_date_elf_end},
        {"/bin/uptime",      _binary_uptime_elf_start,      _binary_uptime_elf_end},
        {"/bin/clear",       _binary_clear_elf_start,       _binary_clear_elf_end},
        {"/bin/time",        _binary_time_elf_start,        _binary_time_elf_end},
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
 * ELF loader
 * ------------------------------------------------------------ */

int elf_load(const void *image, size_t size, uint32_t base_va, struct elf_info *out)
{
    (void) size;

    const Elf32_Ehdr *eh = (const Elf32_Ehdr *)image;

    if (eh->e_ident[0] != 0x7F ||
        eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' ||
        eh->e_ident[3] != 'F')
    {
        kprintf("Invalid ELF magic\n");
        return -1;
    }

    const Elf32_Phdr *ph =
            (const Elf32_Phdr *)((uintptr_t)image + eh->e_phoff);

    uint32_t max_end = 0;
    uint32_t total_copied = 0;

    for (int i = 0; i < eh->e_phnum; i++, ph++)
    {
        if (ph->p_type != PT_LOAD)
            continue;

        uint32_t dest_va = base_va + ph->p_vaddr;
        void *dest = (void *)dest_va;
        const void *src = (const void *)((uintptr_t)image + ph->p_offset);

        k_memcpy(dest, src, ph->p_filesz);
        total_copied += ph->p_filesz;

        if (ph->p_memsz > ph->p_filesz)
        {
            k_memset((uint8_t *)dest + ph->p_filesz, 0,
                     ph->p_memsz - ph->p_filesz);
        }

        uint32_t seg_end = ph->p_vaddr + ph->p_memsz;
        if (seg_end > max_end)
            max_end = seg_end;
    }

    if (out)
    {
        out->base_va = base_va;
        out->entry_va = base_va + eh->e_entry;
        out->max_offset = max_end;
        out->size = total_copied;
        out->environ_off = 0;
        out->curbrk_off = 0;
    }

    /* Parse symbol table for environ / __curbrk */
    if (eh->e_shoff && eh->e_shnum && out)
    {
        const Elf32_Shdr *sh =
                (const Elf32_Shdr *)((uintptr_t)image + eh->e_shoff);

        const Elf32_Shdr *symtab = NULL;
        const Elf32_Shdr *strtab = NULL;

        for (int i = 0; i < eh->e_shnum; i++)
        {
            if (sh[i].sh_type == SHT_SYMTAB)
            {
                symtab = &sh[i];
                if (sh[i].sh_link < eh->e_shnum)
                    strtab = &sh[sh[i].sh_link];
                break;
            }
        }

        if (symtab && strtab && symtab->sh_entsize >= sizeof(Elf32_Sym))
        {
            const Elf32_Sym *symbols =
                    (const Elf32_Sym *)((uintptr_t)image + symtab->sh_offset);
            const char *strings =
                    (const char *)((uintptr_t)image + strtab->sh_offset);

            uint32_t count = symtab->sh_size / symtab->sh_entsize;

            for (uint32_t i = 0; i < count; i++)
            {
                if (symbols[i].st_name >= strtab->sh_size)
                    continue;

                const char *name = strings + symbols[i].st_name;

                if (k_strcmp(name, "environ") == 0)
                {
                    out->environ_off = symbols[i].st_value;
                }
                else if (k_strcmp(name, "__curbrk") == 0)
                {
                    out->curbrk_off = symbols[i].st_value;
                }
            }
        }
    }

    return 0;
}
