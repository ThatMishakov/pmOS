#include "shared_mem.hh"
#include <kernel/errors.h>

shared_mem_map shared_map;

kresult_t release_shared(u64 phys_addr, u64 owner_pid)
{
    if (shared_map.count(phys_addr) == 0) 
        return ERROR_PAGE_NOT_SHARED;

    Shared_Mem_Entry& e = shared_map.at(phys_addr);

    // TODO
    if (e.owners_pid.count(owner_pid) == 0)
        return ERROR_NOT_PAGE_OWNER;

    e.owners_pid.erase(owner_pid);

    if (e.owners_pid.empty())
        shared_map.erase(phys_addr);

    return SUCCESS;
}

kresult_t register_shared(u64 phys_addr, u64 owner)
{
    if (shared_map.count(phys_addr) == 0)
        shared_map.insert({phys_addr, {}});
    
    
    Shared_Mem_Entry& e = shared_map.at(phys_addr);

    if (e.owners_pid.count(owner) != 0)
        return ERROR_ALREADY_PAGE_OWNER;

    e.owners_pid.insert(owner);

    return SUCCESS;
}