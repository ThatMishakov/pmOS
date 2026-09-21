#pragma once
#include "arch_paging.hh"

namespace kernel::m68k::paging
{

class M68030PageTable final: public kernel::m68k::Page_Table {
public:
    virtual void apply() override;

    [[nodiscard]] virtual kresult_t map(phys_addr_t page_addr, void *virt_addr,
                                        Page_Table_Arguments arg) override;

    virtual Page_Info get_page_mapping(void *virt_addr) const override;

    virtual void invalidate_tlb(void *page) override;
    virtual void invalidate_tlb(void *start, size_t size) override;
    virtual void tlb_flush_all() override;

    virtual void invalidate_range(kernel::paging::TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes,
                                  bool free) override;

    static klib::shared_ptr<M68030PageTable> create_empty(unsigned flags = 0);
    virtual klib::shared_ptr<Page_Table> create_clone() override;
private:
    u32 table_root = -1;
};

kresult_t m68030_unmap_kernel_page(kernel::paging::TLBShootdownContext &ctx, void *virt_addr, bool free);
kresult_t m68030_map_page(kernel::paging::ptable_top_ptr_t page_table, phys_addr_t phys_addr, void *virt_addr,
                   kernel::paging::Page_Table_Arguments arg);

void m68030_invalidate_range(phys_addr_t page_table, kernel::paging::TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes,
                                  bool free);

u32 m68030_kernel_page_table();

} // namespace kernel::m68k::paging