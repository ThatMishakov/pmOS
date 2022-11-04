#ifndef SYSTEM_H
#define SYSTEM_H
#include <stdint.h>
#include "../../kernel/include/kernel/syscalls.h"
#include "../../kernel/include/kernel/memory.h"

typedef struct {
    uint64_t result;
    uint64_t value;
} syscall_r;

#if defined(__cplusplus)
extern "C" {
#endif

syscall_r syscall(uint64_t call_n, ...);

syscall_r syscall_get_page(uint64_t addr);

syscall_r map_into_range(uint64_t pid, uint64_t page_start, uint64_t to_addr, uint64_t nb_pages, uint64_t mask);

syscall_r syscall_new_process(uint8_t ring);

syscall_r get_page_multi(uint64_t base, uint64_t nb_pages);

syscall_r start_process(uint64_t pid, uint64_t entry);

syscall_r map_phys(uint64_t virt, uint64_t phys, uint64_t size, uint64_t arg);

#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif