#include "../include/system.h"
#include <stdint.h>

syscall_r syscall_get_page(uint64_t addr)
{
    return syscall(SYSCALL_GET_PAGE, addr);
}

syscall_r map_into_range(uint64_t pid, uint64_t page_start, uint64_t to_addr, uint64_t nb_pages, uint64_t mask)
{
    return syscall(SYSCALL_MAP_INTO_RANGE, pid, page_start, to_addr, nb_pages, mask);
}

syscall_r syscall_new_process(uint8_t ring)
{
    return syscall(SYSCALL_CREATE_PROCESS, ring);
}

syscall_r get_page_multi(uint64_t base, uint64_t nb_pages)
{
    return syscall(SYSCALL_GET_PAGE_MULTI, base, nb_pages);
}

syscall_r start_process(uint64_t pid, uint64_t entry)
{
    return syscall(SYSCALL_START_PROCESS, pid, entry);
}

syscall_r map_phys(uint64_t virt, uint64_t phys, uint64_t size, uint64_t arg)
{
    return syscall(SYSCALL_MAP_PHYS, virt, phys, size, arg);
}

result_t send_message_task(uint64_t pid, uint64_t channel, size_t size, char* message)
{
    return syscall(SYSCALL_SEND_MSG_TASK, pid, channel, size, message).result;
}