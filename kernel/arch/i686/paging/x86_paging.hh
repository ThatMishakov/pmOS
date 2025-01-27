#pragma once
#include <memory/paging.hh>

constexpr u32 PAGE_PRESENT  = 1 << 0;
constexpr u32 PAGE_WRITE    = 1 << 1;
constexpr u32 PAGE_USER     = 1 << 2;
constexpr u32 PAGE_WT       = 1 << 3;
constexpr u32 PAGE_CD       = 1 << 4;
constexpr u32 PAGE_ACCESSED = 1 << 5;
constexpr u32 PAGE_DIRTY    = 1 << 6;
constexpr u32 PAGE_PS       = 1 << 7;
constexpr u32 PAGE_GLOBAL   = 1 << 8;

inline u32 avl_from_page(u32 page) { return (page >> 9) & 0x7; }
inline u32 avl_to_bits(u32 page) { return (page & 0x7) << 9; }

class IA32_Page_Table: public Page_Table
{
public:
    virtual klib::shared_ptr<IA32_Page_Table> create_clone();
    u64 user_addr_max() const override;

    static klib::shared_ptr<IA32_Page_Table> get_page_table(u64 id);
    static bool is_32bit() { return true; }

    static klib::shared_ptr<IA32_Page_Table> create_empty(unsigned flags = 0);

    void apply();

    virtual ~IA32_Page_Table();

protected:
    unsigned cr3 = 0;
};

u64 prepare_pt_for(void *virt_addr, Page_Table_Argumments arg, u32 pt_top_phys);
