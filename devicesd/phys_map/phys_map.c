#include <phys_map/phys_map.h>
#include <stdint.h>
#include <pmos/special.h>
#include <kernel/errors.h>
#include <pmos/system.h>
#include <kernel/flags.h>
#include <pmos/memory.h>

void* map_phys(void* phys_addr, size_t bytes)
{
    uint64_t addr_aligned = (uint64_t)phys_addr & ~(uint64_t)07777;
    uint64_t addr_offset = (uint64_t)phys_addr - addr_aligned;
    uint64_t end_aligned = (uint64_t)phys_addr + bytes;
             end_aligned = (end_aligned & ~(uint64_t)07777) + (end_aligned & (uint64_t)07777 ? 010000 : 0);
    size_t size = end_aligned - addr_aligned;

    mem_request_ret_t t = create_phys_map_region(0, NULL, size, PROT_READ | PROT_WRITE, addr_aligned);

    if (t.result != SUCCESS)
        return NULL;

    return (void*)((uint64_t)t.virt_addr + addr_offset);
}

void unmap_phys(void* virt_addr, size_t bytes)
{
    uint64_t addr_alligned = (uint64_t)virt_addr & ~(uint64_t)0777;
    uint64_t end_alligned = (uint64_t)virt_addr + bytes;
             end_alligned = (end_alligned & ~(uint64_t)0777) + (end_alligned & (uint64_t)0777 ? 01000 : 0);
    size_t size_pages = (end_alligned - addr_alligned) >> 12;
    
    // TODO
}