#ifndef LOAD_ELF_H
#define LOAD_ELF_H
#include <stdint.h>
#include <kernel/elf.h>

struct task_list_node;
uint64_t load_elf(struct task_list_node* addr, uint8_t ring);

#endif