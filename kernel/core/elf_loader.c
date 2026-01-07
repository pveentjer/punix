#include "kernel/kutils.h"
#include "kernel/console.h"
#include "kernel/elf_loader.h"

bool is_elf(const Elf32_Ehdr *elf_header)
{
    return elf_header->e_ident[0] == 0x7F ||
           elf_header->e_ident[1] == 'E'  ||
           elf_header->e_ident[2] == 'L'  ||
           elf_header->e_ident[3] == 'F';
}

int elf_load(const void *image, struct elf_info *elf_info)
{
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *) image;

    // A sanity check to ensure we are loading an actual program and not garbage.
    if (!is_elf(ehdr))
    {
        return -1;
    }

    // it needs to be an executable.
    if (ehdr->e_type != ET_EXEC)
    {
        return -1;
    }

    const Elf32_Phdr *phdr = (const Elf32_Phdr *) ((uintptr_t) image + ehdr->e_phoff);

    uint32_t max_end = 0;
    uint32_t size = 0;

    for (int i = 0; i < ehdr->e_phnum; i++, phdr++)
    {
        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }

        uint32_t dest_va = phdr->p_vaddr;
        const void *src = (const void *) ((uintptr_t) image + phdr->p_offset);

        k_memcpy((void *) dest_va, src, phdr->p_filesz);
        if (phdr->p_memsz > phdr->p_filesz)
        {
            // this is for .bss section which doesn't take size on disk, but we do need to
            // allocate size for it in memory and we also need to zero it.
            void *dst = (void *) (dest_va + phdr->p_filesz);
            uint32_t len = phdr->p_memsz - phdr->p_filesz;
            k_memset(dst, 0, len);
        }

        uint32_t end = phdr->p_vaddr + phdr->p_memsz;
        if (end > max_end)
        {
            max_end = end;
        }

        size += phdr->p_filesz;
    }

    if (elf_info)
    {
        elf_info->base_va = 0;
        elf_info->entry_va = ehdr->e_entry;
        elf_info->max_offset = max_end;
        elf_info->size = size;
        elf_info->environ_off = 0;
        elf_info->curbrk_off = 0;
    }

    return 0;
}

