#include "x86_temp_mapper.hh"
#include <types.hh>

x86_PAE_Temp_Mapper::x86_PAE_Temp_Mapper(void * virt_addr, u64 cr3)
{
    u64 addr = (u64)virt_addr;
    start_index = addr/4096 % 512;

    Page_Table_Argumments arg {
        1,
        0, 
        0, 
        1,
        000
    };

    u64 pt_phys = prepare_pt_for(temp_mapper_get_offset(), arg, cr3);
    Temp_Mapper_Obj<x86_PAE_Entry> tm(request_temp_mapper());
    x86_PAE_Entry *pt = tm.map(pt_phys);

    pt[start_index] = x86_PAE_Entry();
    pt[start_index].present = true;
    pt[start_index].writeable = true;
    pt[start_index].page_ppn = pt_phys >> 12;

    min_index = start_index + 1;
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