#include "kernel/memory.h"
#include "temp_mapper.hh"
#include "paging.hh"
#include "../sched/sched.hh"

constexpr u64 temp_mapper_start_addr = 0177777'776'000'000'000'0000;
u64 temp_mapper_offset = 0;

u64 temp_mapper_get_index(u64 addr)
{
    return (addr/4096) % 512;
}

u64 temp_mapper_get_offset()
{
    return temp_mapper_start_addr + temp_mapper_offset*4096;
}

void * Direct_Mapper::kern_map(u64 phys_frame)
{
    return (void *)(virt_offset + phys_frame);
}

void Direct_Mapper::return_map(void * /* unused */)
{
    // Do nothing and return
    return;
}

// nullptr indicates that the per-CPU mapper must be used
Temp_Mapper *global_temp_mapper = nullptr;
Temp_Mapper &request_temp_mapper() {
    if (global_temp_mapper == nullptr) {
        // Use per-CPU mapper
        return get_cpu_struct()->temp_mapper;
    }

    // Use global mapper (typically during kernel initialization)
    return *global_temp_mapper;
}