#include "shared_mem.hh"
#include <kernel/errors.h>
#include <utils.hh>

shared_mem_map shared_map;

kresult_t release_shared(u64 phys_addr, u64 owner_page_table)
{
    if (shared_map.count(phys_addr) == 0) 
        return ERROR_PAGE_NOT_SHARED;

    Shared_Mem_Entry& e = shared_map.at(phys_addr);

    // TODO
    if (e.owners_pid.count(owner_page_table) == 0)
        return ERROR_NOT_PAGE_OWNER;

    e.owners_pid.erase(owner_page_table);

    if (e.owners_pid.empty())
        shared_map.erase(phys_addr);

    return SUCCESS;
}

kresult_t register_shared(u64 phys_addr, u64 owner_page_table)
{
    // TODO: Implement iterators and make more efficient
    if (shared_map.count(phys_addr) == 0)
        shared_map.insert({phys_addr, {}});
    
    
    Shared_Mem_Entry& e = shared_map.at(phys_addr);

    if (e.owners_pid.count(owner_page_table) != 0)
        return ERROR_ALREADY_PAGE_OWNER;

    e.owners_pid.insert(owner_page_table);

    return SUCCESS;
}

size_t nb_owners(u64 phys_addr)
{
    if (shared_map.count(phys_addr) == 0)
        return 0;

    return shared_map.at(phys_addr).owners_pid.size();
}