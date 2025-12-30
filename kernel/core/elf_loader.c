#include "kernel/kutils.h"
#include "kernel/console.h"
#include "kernel/elf_loader.h"

int elf_load(const void *image, size_t size, uint32_t base_va, struct elf_info *out)
{
    (void)size;
    (void)base_va;

    const Elf32_Ehdr *eh = (const Elf32_Ehdr *)image;

    if (eh->e_ident[0] != 0x7F ||
        eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' ||
        eh->e_ident[3] != 'F')
    {
        return -1;
    }

    if (eh->e_type != ET_EXEC)
    {
        return -1;
    }

    const Elf32_Phdr *ph =
            (const Elf32_Phdr *)((uintptr_t)image + eh->e_phoff);

    uint32_t max_end = 0;
    uint32_t total_copied = 0;

    for (int i = 0; i < eh->e_phnum; i++, ph++)
    {
        if (ph->p_type != PT_LOAD)
        {
            continue;
        }

        uint32_t dest_va = ph->p_vaddr;
        const void *src  = (const void *)((uintptr_t)image + ph->p_offset);

        k_memcpy((void *)dest_va, src, ph->p_filesz);
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