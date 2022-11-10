#pragma once
#include <lib/splay_tree_map.hh>
#include <types.hh>
#include <kernel/memory.h>

struct Shared_Mem_Entry {
    u64 refcount = 0;
};

// phys_addr as key
using shared_mem_map = klib::splay_tree_map<u64, Shared_Mem_Entry>;
extern shared_mem_map shared_map;

// Return PPN
ReturnStr<u64> make_shared(u64 virtual_addr);

// Releases a page that is shared
kresult_t release_shared(u64 phys_addr);

// Referebces a shared page
kresult_t ref_shared(u64 phys_addr);