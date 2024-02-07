#include <pmos/memory.h>

void *__request_mem_b(size_t bytes)
{
    size_t aligned_up = (bytes + 4095) & ~4095UL;
    mem_request_ret_t req = create_normal_region(0, NULL, aligned_up, PROT_READ | PROT_WRITE);
    if (req.result != SUCCESS)
        return NULL;

    return req.virt_addr;
}