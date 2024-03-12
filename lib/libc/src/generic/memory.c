#include <pmos/memory.h>

void *__request_mem_b(size_t bytes)
{
    // dlmalloc does 2 MORECORE calls, the second one is to check if the first one was successful
    static void* next = NULL;
    if (bytes == 0)
        return next;

    size_t aligned_up = (bytes + 4095) & ~4095UL;
    mem_request_ret_t req = create_normal_region(0, NULL, aligned_up, PROT_READ | PROT_WRITE);
    if (req.result != SUCCESS) {
        next = NULL;
        return NULL;
    }

    next = (char *)req.virt_addr + aligned_up;

    return req.virt_addr;
}