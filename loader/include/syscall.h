#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
#include "../../kernel/common/syscalls.h"
#include "../../kernel/common/memory.h"

typedef struct {
    uint64_t result;
    uint64_t value;
} syscall_r;

syscall_r syscall(uint64_t call_n, ...);

inline syscall_r get_page(uint64_t addr)
{
    return syscall(SYSCALL_GET_PAGE, addr);
}

inline syscall_r map_into_range(uint64_t pid, uint64_t page_start, uint64_t to_addr, uint64_t nb_pages, Page_Table_Argumments pta)
{
    return syscall(SYSCALL_MAP_INTO_RANGE, pid, page_start, to_addr, nb_pages, pta);
}

inline syscall_r syscall_new_process(uint8_t ring)
{
    return syscall(SYSCALL_CREATE_PROCESS, ring);
}

inline syscall_r get_page_multi(uint64_t base, uint64_t nb_pages)
{
    return syscall(SYSCALL_GET_PAGE_MULTI, base, nb_pages);
}



#endif