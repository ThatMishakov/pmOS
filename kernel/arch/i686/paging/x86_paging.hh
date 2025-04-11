#pragma once
#include <memory/paging.hh>
#include <pmos/containers/map.hh>

constexpr u64 PAGE_PRESENT  = 1 << 0;
constexpr u64 PAGE_WRITE    = 1 << 1;
constexpr u64 PAGE_USER     = 1 << 2;
constexpr u64 PAGE_WT       = 1 << 3;
constexpr u64 PAGE_CD       = 1 << 4;
constexpr u64 PAGE_ACCESSED = 1 << 5;
constexpr u64 PAGE_DIRTY    = 1 << 6;
constexpr u64 PAGE_PS       = 1 << 7;
constexpr u64 PAGE_GLOBAL   = 1 << 8;

constexpr u64 PAGE_NX = 1ULL << 63;

constexpr u32 _32BIT_ADDR_MASK = ~0xfff;
constexpr u64 PAE_ADDR_MASK    = 0x7ffffffffffff000ULL;

typedef uint64_t pae_entry_t [[gnu::aligned(8)]];

struct PDPTd {
    pae_entry_t entries[4];
};

struct PDPEPage {
    PDPTd pdpts[127];
    u32 next_phys;
    u32 prev_phys;
    u64 bitmap1;
    u64 bitmap2;
    u32 allocated_count;
    u32 padding;
};

static_assert(sizeof(PDPEPage) == 4096, "PDPEPage size is not 4096");

inline u8 avl_from_page(u32 page) { return (page >> 9) & 0x7; }
inline u32 avl_to_bits(u32 page) { return (page & 0x7) << 9; }

class IA32_Page_Table final: public Page_Table
{
public:
    klib::shared_ptr<IA32_Page_Table> create_clone();
    // TODO: Make a pointer...
    void *user_addr_max() const override;

    static klib::shared_ptr<IA32_Page_Table> get_page_table(u64 id);
    static bool is_32bit() { return true; }

    static klib::shared_ptr<IA32_Page_Table> create_empty(unsigned flags = 0);
    static klib::shared_ptr<IA32_Page_Table> capture_initial(u32 cr3);

    void apply();

    virtual ~IA32_Page_Table();

    virtual void invalidate_tlb(void *page) override;
    virtual void invalidate_tlb(void *page, size_t size) override;

    virtual void tlb_flush_all() override;

    virtual kresult_t resolve_anonymous_page(void *virt_addr, unsigned access_type) override;

    virtual kresult_t map(u64 page_addr, void *virt_addr, Page_Table_Arguments arg) override;
    virtual kresult_t map(kernel::pmm::Page_Descriptor page, void *virt_addr,
                          Page_Table_Arguments arg) override;

    virtual kresult_t copy_anonymous_pages(const klib::shared_ptr<Page_Table> &to, void *from_addr,
                                           void *to_addr, size_t size_bytes,
                                           unsigned new_access) override;

    virtual bool is_mapped(void *addr) const override;

    virtual void invalidate(TLBShootdownContext &ctx, void *virt_addr, bool free) override;
    virtual void invalidate_range(TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes,
                                  bool free) override;

    virtual Page_Info get_page_mapping(void *virt_addr) const override;

protected:
    unsigned cr3 = -1;

    using page_table_map = pmos::containers::map<u64, klib::weak_ptr<IA32_Page_Table>>;
    static page_table_map global_page_tables;
    static Spinlock page_table_index_lock;

    static kresult_t insert_global_page_tables(klib::shared_ptr<IA32_Page_Table> table);
    void takeout_global_page_tables();

    void free_user_pages();
};

u64 prepare_pt_for(void *virt_addr, Page_Table_Arguments arg, u32 pt_top_phys);

void free_pae_cr3(u32 cr3);
u32 new_pae_cr3();

// Generic functions to map and release pages in kernel, using the active page table
kresult_t map_kernel_page(u64 phys_addr, void *virt_addr, Page_Table_Arguments arg);
kresult_t unmap_kernel_page(TLBShootdownContext &ctx, void *virt_addr);

kresult_t map_page(ptable_top_ptr_t page_table, u64 phys_addr, void *virt_addr,
                   Page_Table_Arguments arg);

bool detect_nx();

ReturnStr<u32> create_empty_cr3();

extern bool use_pae;
extern bool support_nx;