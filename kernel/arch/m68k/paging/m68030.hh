#pragma once
#include "arch_paging.hh"

namespace kernel::m68k::paging
{

class M68030PageTable final: public kernel::paging::Page_Table {
public:
    virtual void apply() override;

    [[nodiscard]] virtual kresult_t map(phys_addr_t page_addr, void *virt_addr,
                                        Page_Table_Arguments arg) override;

    virtual Page_Info get_page_mapping(void *virt_addr) const override;

    virtual void invalidate_tlb(void *page) override;
    virtual void invalidate_tlb(void *start, size_t size) override;
    virtual void tlb_flush_all() override;

    virtual void invalidate_range(TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes,
                                  bool free) override;
private:
    u32 table_root = -1;
};

kresult_t m68030_map_kernel_page(phys_addr_t phys_addr, void *virt_addr, Page_Table_Arguments arg);
kresult_t m68030_unmap_kernel_page(kernel::paging::TLBShootdownContext &ctx, void *virt_addr);

kresult_t m68030_map_page(ptable_top_ptr_t page_table, phys_addr_t phys_addr, void *virt_addr,
                   Page_Table_Arguments arg);

} // namespace kernel::m68k::paging