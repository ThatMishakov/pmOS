#pragma once
#include <lib/splay_tree_map.hh>
#include <types.h>

struct Shared_Mem_Entry {
    u64 refcount = 0;
};

using shared_mem_map = splay_tree_map<u64, Shared_Mem_Entry>;