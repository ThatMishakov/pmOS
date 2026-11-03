#pragma once

#include <memory/paging.hh>
#include <types.hh>

namespace kernel::aarch64::paging
{

class AArch64_Page_Table final: public ::kernel::paging::Page_Table
{
public:
    static klib::shared_ptr<AArch64_Page_Table> create_empty(int flags = 0);
    klib::shared_ptr<AArch64_Page_Table> create_clone() const;

    static klib::shared_ptr<AArch64_Page_Table> get_page_table(u64 id) noexcept;

    virtual void *user_addr_max() const override;

    void invalidate(::kernel::paging::TLBShootdownContext&, void *virt_addr, bool free) noexcept override;

    virtual kresult_t map(u64 page_addr, void *virt_addr,
                          ::kernel::paging::Page_Table_Arguments arg) override;

    void invalidate_tlb(void *page) override;
    void invalidate_tlb(void *start, size_t size) override;
    void tlb_flush_all() override;

    Page_Info get_page_mapping(void *virt_addr) const override;

    virtual void invalidate_range(::kernel::paging::TLBShootdownContext&, void *virt_addr, size_t size_bytes,
                                  bool free) override;

    ~AArch64_Page_Table();

    bool is_32bit() const;
    void apply();

private:
    u64 page_directory = 0;
};

} // namespace kernel::aarch64::paging