#ifndef LOAD_ELF_H
#define LOAD_ELF_H
#include <stdint.h>
#include "../../kernel/common/elf.h"

uint64_t load_elf(ELF_64bit* addr, uint8_t ring);

#endif