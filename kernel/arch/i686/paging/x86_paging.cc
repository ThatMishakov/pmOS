#include "x86_paging.hh"

#include <x86_asm.hh>
#include <memory/pmm.hh>

using namespace kernel;

void apply_page_table(ptable_top_ptr_t page_table) { setCR3(page_table); }

void IA32_Page_Table::apply() { apply_page_table(cr3); }

bool use_pae    = false;
bool support_nx = false;

kresult_t ia32_map_page(u32 cr3, u64 phys_addr, void *virt_addr, Page_Table_Argumments arg)
{
    assert(!(phys_addr & 0xFFF));
    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *active_pt = mapper.map(cr3);

        auto pd_idx = (u32(virt_addr) >> 22) & 0x3FF;
        auto pt_idx = (u32(virt_addr) >> 12) & 0x3FF;

        auto &pd_entry = active_pt[pd_idx];
        if (!(pd_entry & PAGE_PRESENT)) {
            auto new_pt_phys = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_pt_phys))
                return -ENOMEM;

            clear_page(new_pt_phys);

            u32 new_pt = new_pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            pd_entry   = new_pt;
        }

        Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());
        u32 *pt = pt_mapper.map(pd_entry & 0xFFFFF000);

        auto &pt_entry = pt[pt_idx];
        if (pt_entry & PAGE_PRESENT)
            return -EEXIST;

        u32 new_pt = phys_addr;
        if (arg.writeable)
            new_pt |= PAGE_WRITE;
        if (arg.user_access)
            new_pt |= PAGE_USER;
        if (arg.global)
            new_pt |= PAGE_GLOBAL;
        new_pt |= avl_to_bits(arg.extra);
        if (arg.cache_policy != Memory_Type::Normal)
            new_pt |= PAGE_CD;

        pt_entry = new_pt;
        return 0;
    } else {
        // TODO
        return -ENOSYS;
    }
}

kresult_t map_kernel_page(u64 phys_addr, void *virt_addr, Page_Table_Argumments arg)
{
    assert(!arg.user_access);
    const u32 cr3 = getCR3();
    return ia32_map_page(cr3, phys_addr, virt_addr, arg);
}