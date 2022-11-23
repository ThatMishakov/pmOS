#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H
#include <stdint.h>

typedef struct {
    uint32_t    magic;
    uint8_t     bitness;
    uint8_t     endiannes;
    uint8_t     header_version;
    uint8_t     os_abi;
    uint64_t    padding;
    uint16_t    type;
    uint16_t    instr_set;
    uint32_t    elf_ver;
    uint64_t    program_entry;
    uint64_t    program_header;
    uint64_t    section_header;
    uint32_t    flags;
    uint16_t    header_size;
    uint16_t    prog_header_size;
    uint16_t    program_header_entries;
    uint16_t    section_header_entry_size;
    uint16_t    section_header_entries;
    uint16_t    section_header_names_index;
} ELF_64bit;

#define ELF_X86     3
#define ELF_64BIT   2

typedef struct {
    uint32_t    type;
    uint32_t    flags;
    uint64_t    p_offset;
    uint64_t    p_vaddr;
    uint64_t    undefined;
    uint64_t    p_filesz;
    uint64_t    p_memsz;
    uint64_t    allignment;
} ELF_PHeader_64;

#define ELF_SEGMENT_LOAD    1
#define ELF_SEGNEMT_DYNAMIC 2

#define ELF_FLAG_EXECUTABLE 1
#define ELF_FLAG_WRITABLE   2
#define ELF_FLAG_READABLE   4

#endif