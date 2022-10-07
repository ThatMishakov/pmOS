#ifndef KERNEL_LOADER_H
#define KERNEL_LOADER_H
#include <stdint.h>

void load_kernel(uint64_t multiboot_info_str);

#endif