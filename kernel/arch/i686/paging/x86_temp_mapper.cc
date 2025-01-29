#include "x86_temp_mapper.hh"

#include "x86_paging.hh"

#include <x86_asm.hh>

static x86_PAE_Temp_Mapper pae_mapper;
static x86_2level_Mapper two_level_mapper;

extern bool use_pae;

Temp_Mapper *get_temp_temp_mapper(void *addr, u32 kernel_cr3)
{
    if (use_pae) {
        pae_mapper = x86_PAE_Temp_Mapper(addr, kernel_cr3);
        return &pae_mapper;
    } else {
        two_level_mapper = x86_2level_Mapper(addr, kernel_cr3);
        return &two_level_mapper;
    }
}

Temp_Mapper *create_temp_mapper(void *virt_addr, u32 cr3)
{
    if (!use_pae) {
        return new x86_2level_Mapper(virt_addr, cr3);
    } else {
        return new x86_PAE_Temp_Mapper(virt_addr, cr3);
    }
}

x86_2level_Mapper::x86_2level_Mapper(void *virt_addr, u32 cr3)
{
    pt_mapped = (u32 *)virt_addr;

    u32 addr    = (u32)virt_addr;
    start_index = addr / 4096 % 1024;

    Page_Table_Argumments arg {1, 1, 0, 0, 1, 000};

    auto pt_phys = prepare_pt_for(virt_addr, arg, cr3);
    if (pt_phys == -1UL)
        panic("Can't initialize temp mapper");

    Temp_Mapper_Obj<u32> tm(request_temp_mapper());
    u32 *pt = tm.map(pt_phys);

    u32 entry = 0;
    entry |= pt_phys;
    entry |= PAGE_PRESENT;
    entry |= PAGE_WRITE;
    pt[start_index] = entry;

    min_index = start_index + 1;
}

void *x86_2level_Mapper::kern_map(u64 phys_frame)
{
    assert(!(phys_frame >> 32));
    for (unsigned i = min_index; i < start_index + size; ++i) {
        if (not(pt_mapped[i] & PAGE_PRESENT)) {
            u32 entry = 0;
            entry |= phys_frame;
            entry |= PAGE_PRESENT;
            entry |= PAGE_WRITE;
            pt_mapped[i] = entry;

            min_index = i + 1;

            char *t = (char *)pt_mapped;
            t += (i - start_index) * 4096;

            return (void *)t;
        }
    }

    return nullptr;
}

void x86_2level_Mapper::return_map(void *p)
{
    if (p == nullptr)
        return;

    u64 i          = (u64)p;
    unsigned index = temp_mapper_get_index(i);

    pt_mapped[index] = 0;
    if (index < min_index)
        min_index = index;

    invlpg(p);
}

u32 x86_2level_Mapper::temp_mapper_get_index(u32 addr) { return (addr / 4096) % 1024; }