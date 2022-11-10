#pragma once
#include <lib/splay_tree_map.hh>
#include <lib/set.hh>
#include <types.hh>
#include <kernel/memory.h>

struct Shared_Mem_Entry {
    klib::set<u64> owners_pid;
};

// phys_addr as key
using shared_mem_map = klib::splay_tree_map<u64, Shared_Mem_Entry>;
extern shared_mem_map shared_map;

// Return PPN
ReturnStr<u64> make_shared(u64 virtual_addr, u64 owner_pid);

// Releases a page that is shared
kresult_t release_shared(u64 phys_addr, u64 owner_pid);

// References a shared page
kresult_t ref_shared(u64 phys_addr, u64 owner);

// Registers a page as shared for owner
kresult_t register_shared(u64 phys_addr, u64 owner);