#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H
#include <stdint.h>

typedef struct {
    uint32_t    magic;
    uint8_t     bitness;
    uint8_t     endianness;
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
#define ELF_RISCV   0xF3
#define ELF_X86_64  0x3E
#define ELF_AARCH64 0xB7

#define ELF_EXEC    2

#define ELF_MAGIC   0x464C457F

#define ELF_LITTLE_ENDIAN 1

#define ELF_ENDIANNESS  ELF_LITTLE_ENDIAN

#ifdef __x86_64__
#define ELF_BITNESS ELF_64BIT
#define ELF_INSTR_SET ELF_X86_64
#elif defined(__i386__)
#define ELF_BITNESS ELF_32BIT
#define ELF_INSTR_SET ELF_X86
#elif defined(__riscv)
#define ELF_BITNESS ELF_64BIT
#define ELF_INSTR_SET ELF_RISCV
#elif defined(__aarch64__)
#define ELF_BITNESS ELF_64BIT
#define ELF_INSTR_SET ELF_AARCH64
#endif

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
#define STT_TLS             6
#define PT_TLS              7

#define ELF_FLAG_EXECUTABLE 1
#define ELF_FLAG_WRITABLE   2
#define ELF_FLAG_READABLE   4

#endif