#include "../../include/kernel/kutils.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/kutils.h"  // (you can drop this duplicate if you want)

/* ELF binary symbols from objcopy */
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


const struct embedded_app embedded_apps[] = {
        {"/bin/sh",          _binary_sh_elf_start,          _binary_sh_elf_end},
        {"/bin/loop",        _binary_loop_elf_start,        _binary_loop_elf_end},
        {"/bin/ps",          _binary_ps_elf_start,          _binary_ps_elf_end},
        {"/bin/test_args",   _binary_test_args_elf_start,   _binary_test_args_elf_end},
        {"/bin/spawn_chain", _binary_spawn_chain_elf_start, _binary_spawn_chain_elf_end},
        {"/bin/kill",        _binary_kill_elf_start,        _binary_kill_elf_end},
        {"/bin/ls",          _binary_ls_elf_start,          _binary_ls_elf_end},
        {"/bin/cat",         _binary_cat_elf_start,         _binary_cat_elf_end},
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
    de->d_ino    = ino;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type   = DT_REG;

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
                             (uint32_t)(i + 1));  // fake inode
        idx++;
    }

    return (int)idx;
}

/* --------------------------------------------------------------------
 * ELF loader with relocation fixups
 * -------------------------------------------------------------------- */

bool elf_load(const void *image, size_t size, uint32_t load_base, struct elf_info *out)
{
    (void) size;

    const Elf32_Ehdr *eh = (const Elf32_Ehdr *) image;

    // Basic ELF magic check
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
    {
        kprintf("Invalid ELF magic\n");
        return false;
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
    }

    return true;
}
