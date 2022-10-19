#ifndef LOAD_ELF_H
#define LOAD_ELF_H
#include <stdint.h>

uint64_t load_elf(uint64_t addr, uint8_t ring);

#endif