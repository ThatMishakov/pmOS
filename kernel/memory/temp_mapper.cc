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

x86_PAE_Temp_Mapper::x86_PAE_Temp_Mapper()
{
    start_index = temp_mapper_get_index(temp_mapper_get_offset());
    pt_mapped = (PTE *)temp_mapper_get_offset();

    Page_Table_Argumments arg {
        1,
        0, 
        0, 
        1,
        000
    };

    PTE* pt = (PTE *)rec_prepare_pt_for(temp_mapper_get_offset(), arg);
    pt[start_index] = PTE();
    pt[start_index].present = true;
    pt[start_index].writeable = true;
    pt[start_index].page_ppn = rec_get_pt_ppn(temp_mapper_get_offset());


    min_index = start_index + 1;

    temp_mapper_offset += size;
}

void * x86_PAE_Temp_Mapper::kern_map(u64 phys_frame)
{
    for (unsigned i = min_index; i < start_index+size; ++i) {
        if (not pt_mapped[i].present) {
            pt_mapped[i].present = true;
            pt_mapped[i].writeable = true;
            pt_mapped[i].page_ppn = phys_frame >> 12;

            min_index = i+1;

            char *t = (char *)pt_mapped;
            t += (i - start_index)*4096;

            return (void *)t;
        }
    }
    
    return nullptr;
}

void * Direct_Mapper::kern_map(u64 phys_frame)
{
    return (void *)(virt_offset + (phys_frame << 12));
}

void Direct_Mapper::return_map(void * /* unused */)
{
    // Do nothing and return
    return;
}

void x86_PAE_Temp_Mapper::return_map(void * p)
{
    if (p == nullptr)
        return;

    u64 i = (u64)p;
    unsigned index = temp_mapper_get_index(i);

    pt_mapped[index] = PTE();
    if (index < min_index)
        min_index = index;

    invlpg(i);
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