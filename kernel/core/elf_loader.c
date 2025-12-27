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
        if (k_strcmp(embedded_apps[i].name, name) == 0)
            return &embedded_apps[i];
    return NULL;
}

int elf_load(const void *image, size_t size, uint32_t base_va, struct elf_info *out)
{
    (void)size;
    (void)base_va;

    kprintf("elf_load\n");

    const Elf32_Ehdr *eh = (const Elf32_Ehdr *)image;

    if (eh->e_ident[0] != 0x7F ||
        eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' ||
        eh->e_ident[3] != 'F')
    {
        kprintf("Not elf\n");
        return -1;
    }

    if (eh->e_type != ET_EXEC)
    {
        kprintf("Not ET_XEC\n");
        return -1;
    }

    const Elf32_Phdr *ph =
            (const Elf32_Phdr *)((uintptr_t)image + eh->e_phoff);

    uint32_t max_end = 0;
    uint32_t total_copied = 0;

    kprintf("e_phnum: %u\n", eh->e_phnum);
    kprintf("e_type=%u e_phoff=%u e_phnum=%u\n", eh->e_type, eh->e_phoff, eh->e_phnum);

    for (int i = 0; i < eh->e_phnum; i++, ph++)
    {
        if (ph->p_type != PT_LOAD)
        {
            kprintf("ph->p_type: %u\n", ph->p_type);
            continue;
        }

        uint32_t dest_va = ph->p_vaddr;
        const void *src  = (const void *)((uintptr_t)image + ph->p_offset);

        kprintf("ELF LOAD:\n");
        kprintf("  image        = 0x%08x\n", (uint32_t)image);
        kprintf("  p_vaddr      = 0x%08x\n", ph->p_vaddr);
        kprintf("  p_offset     = 0x%08x\n", ph->p_offset);
        kprintf("  p_filesz     = 0x%08x\n", ph->p_filesz);
        kprintf("  p_memsz      = 0x%08x\n", ph->p_memsz);
        kprintf("  dest_va      = 0x%08x\n", dest_va);
        kprintf("  src          = 0x%08x\n", (uint32_t)src);
        kprintf("  dest_end     = 0x%08x\n", dest_va + ph->p_filesz);

        kprintf("k_memcpy start\n");
        k_memcpy((void *)dest_va, src, ph->p_filesz);
        kprintf("k_memcpy end\n");
        if (ph->p_memsz > ph->p_filesz)
            k_memset((void *)(dest_va + ph->p_filesz), 0,
                     ph->p_memsz - ph->p_filesz);

        uint32_t end = ph->p_vaddr + ph->p_memsz;
        if (end > max_end)
            max_end = end;

        total_copied += ph->p_filesz;
    }

    if (out)
    {
        out->base_va     = 0;
        out->entry_va    = eh->e_entry;
        out->max_offset  = max_end;
        out->size        = total_copied;
        out->environ_off = 0;
        out->curbrk_off  = 0;
    }

    return 0;
}
