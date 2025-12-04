#include "../../include/kernel/kutils.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/kutils.h"

/* ELF binary symbols from objcopy */
extern unsigned char _binary_init_elf_start[];
extern unsigned char _binary_init_elf_end[];
extern unsigned char _binary_loop_elf_start[];
extern unsigned char _binary_loop_elf_end[];
extern unsigned char _binary_ps_elf_start[];
extern unsigned char _binary_ps_elf_end[];
extern unsigned char _binary_test_args_elf_start[];
extern unsigned char _binary_test_args_elf_end[];

const struct embedded_app embedded_apps[] = {
        { "/sbin/init",     _binary_init_elf_start,      _binary_init_elf_end },
        { "/bin/loop",      _binary_loop_elf_start,      _binary_loop_elf_end },
        { "/bin/ps",        _binary_ps_elf_start,        _binary_ps_elf_end },
        { "/bin/test_args", _binary_test_args_elf_start, _binary_test_args_elf_end },
};

const size_t embedded_app_count =
        sizeof(embedded_apps) / sizeof(embedded_apps[0]);

const struct embedded_app *find_app(const char *name)
{
    for (size_t i = 0; i < embedded_app_count; i++) {
        if (k_strcmp(embedded_apps[i].name, name) == 0)
            return &embedded_apps[i];
    }
    return 0;
}


/**
 * Load an ELF binary image from memory and return its entry point.
 */
uint32_t elf_load(const void *image, size_t size, uint32_t load_base)
{
    (void)size;
    (void)load_base;   // no base offset for now

    const Elf32_Ehdr *eh = (const Elf32_Ehdr *)image;

    // sanity check ELF header
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        panic("Invalid ELF magic");
    }

    const Elf32_Phdr *ph =
            (const Elf32_Phdr *)((uintptr_t)image + eh->e_phoff);

    for (int i = 0; i < eh->e_phnum; i++, ph++) {
        if (ph->p_type != PT_LOAD)
            continue;

        void *dest        = (void *)(ph->p_vaddr);
        const void *src   = (const void *)((uintptr_t)image + ph->p_offset);

        k_memcpy(dest, src, ph->p_filesz);

        if (ph->p_memsz > ph->p_filesz) {
            k_memset((uint8_t *)dest + ph->p_filesz, 0,
                     ph->p_memsz - ph->p_filesz);
        }
    }

    return eh->e_entry;
}

