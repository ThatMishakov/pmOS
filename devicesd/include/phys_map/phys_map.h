#ifndef PHYS_MAP_H
#define PHYS_MAP_H
#include <stdint.h>
#include <stdlib.h>

void* map_phys(void* phys_addr, size_t bytes);
void unmap_phys(void* virt_addr, size_t bytes);


#endif