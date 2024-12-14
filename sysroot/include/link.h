#ifndef _LINK_H
#define _LINK_H

#ifdef __cplusplus
extern "C" {
#endif

#define __DECLARE_SIZE_T
#include "__posix_types.h"

#include <elf.h>

#if defined(__x86_64__) || defined(__aarch64__) || (defined(__riscv) && __riscv_xlen == 64)
    #define ElfW(type) Elf64_##type
#else
    #define ElfW(type) Elf32_##type
#endif

#define Elf_Addr ElfW(Addr)
#define Elf_Phdr ElfW(Phdr)
#define Elf_Half ElfW(Half)
#define Elf_Dyn  ElfW(Dyn)

struct dl_phdr_info {
    Elf_Addr dlpi_addr;
    const char *dlpi_name;
    const Elf_Phdr *dlpi_phdr;
    Elf_Half dlpi_phnum;
    // unsigned long long int dlpi_adds;
    // unsigned long long int dlpi_subs;
    // size_t dlpi_tls_modid;
    // void *dlpi_tls_data;
};

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data),
                    void *data);

#ifdef __cplusplus
}
#endif

#endif // _LINK_H