#include "riscv64_temp_mapper.hh"
#include <memory/temp_mapper.hh>
#include <memory/paging.hh>
#include "riscv64_paging.hh"

RISCV64_Temp_Mapper::RISCV64_Temp_Mapper(void *virt_addr, u64 pt_ptr)
{
    pt_mapped = (RISCV64_PTE *)virt_addr;
    
    u64 addr = (u64)virt_addr;
    start_index = addr/4096 % 512;

    Page_Table_Argumments arg {
        1,
        1,
        0, 
        0, 
        1,
        000
    };

    u64 leaf_pt_phys = prepare_leaf_pt_for(virt_addr, arg, pt_ptr);
    Temp_Mapper_Obj<RISCV64_PTE> tm(request_temp_mapper());
    RISCV64_PTE *pt = tm.map(leaf_pt_phys);

    RISCV64_PTE pte = RISCV64_PTE();
    pte.valid = true;
    pte.readable = true;
    pte.writeable = true;
    pte.ppn = leaf_pt_phys >> 12;
    pt[start_index] = pte;

    min_index = start_index + 1;
}

void * RISCV64_Temp_Mapper::kern_map(u64 phys_frame)
{
    for (unsigned i = min_index; i < start_index+size; ++i) {
        if (not pt_mapped[i].valid) {
            RISCV64_PTE pte = RISCV64_PTE();
            pte.valid = true;
            pte.readable = true;
            pte.writeable = true;
            pte.ppn = phys_frame >> 12;
            pt_mapped[i] = pte;

            min_index = i+1;
            char *t = (char *)pt_mapped;
            t += (i - start_index)*4096;

            // I've seen somewhere in RISC-V spec that the implementations
            // may cache the unmapped entries as well, so flush here wouldn't
            // hurt
            flush_page((void*)t);

            return (void *)t;
        }
    }

    return nullptr;
}

static u64 temp_mapper_get_index(u64 addr)
{
    return (addr/4096) % 512;
}

void RISCV64_Temp_Mapper::return_map(void * p)
{
    if (p == nullptr)
        return;

    u64 i = (u64)p;
    unsigned index = temp_mapper_get_index(i);

    pt_mapped[index] = RISCV64_PTE();
    if (index < min_index)
        min_index = index;

    flush_page(p);
}