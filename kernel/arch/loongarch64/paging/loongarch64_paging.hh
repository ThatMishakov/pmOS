#pragma once
#include <memory/mem_object.hh> // ???
#include <memory/paging.hh>
#include <types.hh>

constexpr u64 PAGE_VALID      = 0x01;
constexpr u64 PAGE_DIRTY      = 0x02;
constexpr u64 PAGE_USER_MASK  = 0x0c;
constexpr u64 PAGE_MAT_CC     = ((u64)1 << 4);
constexpr u64 PAGE_MAT_WUC    = ((u64)1 << 5);
constexpr u64 PAGE_MAT_MASK   = 0x30;
constexpr u64 PAGE_PRESENT    = 0x80;
constexpr u64 PAGE_WRITEABLE  = 0x0100;
constexpr u64 PAGE_NO_READ    = (1UL << 61);
constexpr u64 PAGE_NO_EXECUTE = (1UL << 62);
constexpr u64 PAGE_RPLV       = (1UL << 63);

constexpr unsigned PAGE_AVAILABLE_SHIFT = 8;
constexpr u64 PAGE_AVAILABLE_MASK       = (0xf << 8);

constexpr u64 PAGE_ADDR_MASK = 0x1ffffffffffff000UL;

// This is programable, but just use tbe normel x86/risc-v style page tables
constexpr unsigned page_table_entries = 9;
constexpr unsigned page_idx_mask      = (1 << page_table_entries) - 1;
constexpr unsigned paging_l1_offset   = 12 + 9 * 0;
constexpr unsigned paging_l2_offset   = 12 + 9 * 1;
constexpr unsigned paging_l3_offset   = 12 + 9 * 2;
constexpr unsigned paging_l4_offset   = 12 + 9 * 3;

u64 loongarch_cache_bits(Memory_Type);
Memory_Type pte_cache_policy(u64);

class LoongArch64_Page_Table final: public Page_Table
{
public:
    // Gets the page table by its ID
    static klib::shared_ptr<LoongArch64_Page_Table> get_page_table(u64 id) noexcept;

    /**
     * @brief Creates a clone of this page table
     *
     * This function creates a new page table, which is a 'clone' of this page
     * table. The clone references the same memory objects and the same memory
     * regions, with the memory layout being the same in such a way that the new
     * task using the page table will see the same memory layout and contents as
     * the original task. This function is somewhat abstract as it doesn't
     * actually have to clone the page tables, but can transform the memory into
     * copy on write, or do other things, as long as the previous condition is
     * met.
     *
     * @return klib::shared_ptr<LoongArch64_Page_Table> A clone of the page table
     */
    klib::shared_ptr<LoongArch64_Page_Table> create_clone();

    /**
     * @brief Creates an empty page table
     *
     * This function creates a new empty page table. The correct kernel tables
     * are inherited, and the userspace has empty address space with no mappings
     * and memory regions.
     *
     * @return klib::shared_ptr<LoongArch64_Page_Table> A pointer to the new page
     * table
     */
    static klib::shared_ptr<LoongArch64_Page_Table> create_empty(int flags = 0);

    /// Installs the page table
    void apply() noexcept;

    /// @brief Maximum address that the user space is allowed to use
    /// @return u64 the number, addresses before which are allowed for user
    /// space
    virtual void *user_addr_max() const override;

    /// Invalidate (return) the page table entry for the given virtual address.
    /// If free is set to true, the page is also returned to the page frame
    /// allocator
    /// @param virt_addr Virtual address of the page
    /// @param free Whether the page needs to be freed
    void invalidate(TLBShootdownContext &ctx, void *virt_addr, bool free) noexcept override;

    virtual kresult_t map(u64 page_addr, void *virt_addr, Page_Table_Argumments arg) override;
    virtual kresult_t map(kernel::pmm::Page_Descriptor page, void *virt_addr,
                          Page_Table_Argumments arg) override;

    kresult_t resolve_anonymous_page(void *virt_addr, unsigned access_type) override;

    // Clears the TLB cache for the given page
    void invalidate_tlb(void *page) override;
    void invalidate_tlb(void *start, size_t size) override;
    void tlb_flush_all() override;

    Page_Info get_page_mapping(void *virt_addr) const override;

    virtual kresult_t copy_anonymous_pages(const klib::shared_ptr<Page_Table> &to, void *from_addr,
                                           void *to_addr, size_t size_bytes,
                                           unsigned new_access) override;

    virtual void invalidate_range(TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes,
                                  bool free) override;

    bool is_32bit() const { return flags & FLAG_32BIT; }

    ~LoongArch64_Page_Table();

protected:
    LoongArch64_Page_Table() = default;
    LoongArch64_Page_Table(int flags): flags(flags) {}

    static constexpr u64 PAGE_DIRECTORY_INITIALIZER = -1UL;

    u64 page_directory = PAGE_DIRECTORY_INITIALIZER;
    int flags          = 0;

    static kresult_t insert_global_page_tables(klib::shared_ptr<LoongArch64_Page_Table>);
    void takeout_global_page_tables();

    void free_all_pages();
};

/// Maps a page (kernel or user) to the specified location. The right page directory (low or high)
/// has to be supplied, depending on the priviledge level
kresult_t loongarch_map_page(u64 pt_top_phys, void *virt_addr, u64 phys_addr,
                             Page_Table_Argumments arg);

/// Unmaps the page, freeing it as needed.
kresult_t loongarch_unmap_page(TLBShootdownContext &ctx, u64 pt_top_phys, void *virt_addr,
                               bool free = false);

// Generic functions to map and release pages in kernel, using the active page table
kresult_t map_kernel_page(u64 phys_addr, void *virt_addr, Page_Table_Argumments arg);
kresult_t unmap_kernel_page(TLBShootdownContext &ctx, void *virt_addr);
