#include <link.h>
#include <stdio.h>
#include <errno.h>
#include <pmos/load_data.h>
#include <stdlib.h>

struct elf_module {
    Elf_Addr dlpi_addr;
    const char *dlpi_name;
    const Elf_Phdr *dlpi_phdr;
    Elf_Half dlpi_phnum;

    // TODO I guess?
};

static int elf_module_size = 0;
static int elf_module_capacity = 0;
struct elf_module *elf_modules = NULL;

#define max(x, y) (x > y ? x : y)

int elf_modules_ensure_capacity(int additional)
{
    if (elf_module_size + additional <= elf_module_capacity)
        return 0;

    int new_capacity = max(elf_module_capacity * 2, elf_module_size + additional);
    struct elf_module *new_modules = realloc(elf_modules, new_capacity * sizeof(struct elf_module));
    if (new_modules == NULL) {
        fprintf(stderr, "pmOS libC: Failed to allocate memory for ELF modules\n");
        return -ENOMEM;
    }

    elf_modules = new_modules;
    elf_module_capacity = new_capacity;
    return 0;
}

extern void *__load_data_kernel;
extern size_t __load_data_size_kernel;

extern void *__phdr_addr;
extern size_t __phdr_size;
extern size_t __phdr_num;

void __libc_init_dyn()
{
    if (elf_modules_ensure_capacity(1) < 0)
        return;

    // TODO: Find the dlpi_addr offset from the PHDR
    // Since the ELF loader doesn't rellocate for the time being, leave it as 0
    size_t dlpi_addr = 0; // So, replace this...

    struct elf_module *m = &elf_modules[elf_module_size++];
    m->dlpi_addr = dlpi_addr;
    m->dlpi_phdr = (const Elf_Phdr *)__phdr_addr;
    m->dlpi_phnum = __phdr_num;
    m->dlpi_name = NULL;
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data)
{
    int ret = 0;
    for (int i = 0; i < elf_module_size && !ret; i++) {
        struct dl_phdr_info info = {
            .dlpi_addr = elf_modules[i].dlpi_addr,
            .dlpi_name = elf_modules[i].dlpi_name ? elf_modules[i].dlpi_name : "",
            .dlpi_phdr = elf_modules[i].dlpi_phdr,
            .dlpi_phnum = elf_modules[i].dlpi_phnum
        };
        ret = callback(&info, sizeof(struct dl_phdr_info), data);
    }
    return ret;
}