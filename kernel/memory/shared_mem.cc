#include "shared_mem.hh"
#include <kernel/errors.h>

shared_mem_map shared_map;

kresult_t release_shared(u64 phys_addr)
{
    if (shared_map.count(phys_addr) == 0) 
        return ERROR_PAGE_NOT_SHARED;

    Shared_Mem_Entry& e = shared_map.at(phys_addr);

    e.refcount--;

    if (e.refcount == 0)
        shared_map.erase(phys_addr);

    return SUCCESS;
}