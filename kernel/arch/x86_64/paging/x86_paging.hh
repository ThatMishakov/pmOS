/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <memory/paging.hh>

namespace kernel::x86_64::paging
{

/// @brief Indicates NX (no execute) bit is supported and enabled
/// @todo Very x86-specific
extern bool nx_bit_enabled;

/** @brief x86 paging structure, used in PAE, 4 level and 5 level paging
 *
 * This structure is by the x86 CPUs to translate the physical addresses. Each
 * table contains 512 entries and is page aligned. Unless accessing the last
 * level of paging (Page Table itself) or using huge pages, entries point to the
 * next levels of directories untill the end is reached. A very good explanation
 * in https://wiki.osdev.org/Paging
 */
struct x86_PAE_Entry {
    u8 present : 1            = 0;
    u8 writeable : 1          = 0;
    u8 user_access : 1        = 0;
    u8 write_through : 1      = 0;
    u8 cache_disabled : 1     = 0;
    u8 accessed : 1           = 0;
    u8 dirty : 1              = 0;
    u8 pat_size : 1           = 0;
    u8 global : 1             = 0;
    u8 avl : 3                = 0; // Avaliable
    u64 page_ppn : 40         = 0;
    u64 avl2 : 7              = 0;
    u8 pk : 4                 = 0;
    u8 execution_disabled : 1 = 0;

    /// Clears the entry (sets everything to 0, effectively invalidating it),
    /// freeing the page unless the no release flag is set.
    void clear_auto();

    /// Clears the entry without doing any checks
    void clear_nofree();
} PACKED ALIGNED(8);

class x86_Page_Table: public kernel::paging::Page_Table
{
public:
    virtual u64 get_cr3() const = 0;

    /// True if the page table is used by the current task
    bool is_active() const;

    /// True if the page table is used by other processors
    bool is_used_by_others() const;

    /// Atomically incerement and decrement active counter
    void atomic_active_sum(u64 val) noexcept;

    virtual void invalidate_tlb(void *page) override;
    virtual void invalidate_tlb(void *page, size_t size) override;

    virtual void tlb_flush_all() override;

protected:
    virtual void free_user_pages() = 0;

    volatile u64 active_count = 0;
};

class x86_4level_Page_Table: public x86_Page_Table
{
public:
    u64 pml4_phys = -1UL;

    virtual u64 get_cr3() const override { return (u64)pml4_phys; }

    /**
     * @brief Creates an initial page table, capturing pointer to the root level
     * @param root Pointer to the root level of the page table
     * @return klib::shared_ptr<RISCV64_Page_Table> A shared pointer to the page
     * table
     */
    static klib::shared_ptr<x86_4level_Page_Table> capture_initial(u64 root);

    static klib::shared_ptr<x86_4level_Page_Table> create_empty(int flags = 0);

    virtual kresult_t copy_anonymous_pages(const klib::shared_ptr<Page_Table> &to, void *from_addr,
                                           void *to_addr, size_t size_bytes,
                                           unsigned new_access) override;

    // Maps the page with the appropriate permissions
    virtual kresult_t map(u64 page_addr, void *virt_addr,
                          Page_Table_Arguments arg) noexcept override;

    virtual kresult_t map(kernel::pmm::Page_Descriptor page, void *virt_addr,
                          Page_Table_Arguments arg) noexcept override;

    virtual void invalidate(kernel::paging::TLBShootdownContext &ctx, void *virt_addr,
                            bool free) override;

    bool is_mapped(void *virt_addr) const override;

    virtual ~x86_4level_Page_Table();

    void *user_addr_max() const override
    {
        return (void *)((flags & FLAG_32BIT) ? 0x100000000 : 0x800000000000);
    }

    static inline unsigned pt_index(void *addr)
    {
        u64 index = (u64)addr >> 12;
        return index & 0777;
    }

    static inline unsigned pd_index(void *addr)
    {
        u64 index = (u64)addr >> (12 + 9);
        return index & 0777;
    }

    static inline unsigned pdpt_index(void *addr)
    {
        u64 index = (u64)addr >> (12 + 9 + 9);
        return index & 0777;
    }

    static inline unsigned pml4_index(void *addr)
    {
        u64 index = (u64)addr >> (12 + 9 + 9 + 9);
        return index & 0777;
    }

    Page_Info get_page_mapping(void *virt_addr) const override;

    klib::shared_ptr<x86_4level_Page_Table> create_clone();

    virtual ReturnStr<bool> atomic_copy_to_user(void *to, const void *from, size_t size) override;

    constexpr static u64 l4_align = 4096UL * 512 * 512 * 512;
    constexpr static u64 l3_align = 4096UL * 512 * 512;
    constexpr static u64 l2_align = 4096UL * 512;
    constexpr static u64 l1_align = 4096UL;

    static klib::shared_ptr<x86_4level_Page_Table> get_page_table(u64 id);

    void apply() noexcept;

    bool is_32bit() const { return flags & FLAG_32BIT; }

    kresult_t resolve_anonymous_page(void *virt_addr, unsigned access_type) override;

protected:
    /// @brief Inserts the page table into the map of the page tables
    static kresult_t insert_global_page_tables(klib::shared_ptr<x86_4level_Page_Table> table);

    /// @brief Takes out this page table from the map of the page tables
    void takeout_global_page_tables();

    using page_table_map = klib::splay_tree_map<u64, klib::weak_ptr<x86_4level_Page_Table>>;
    /// @brief Map holding all the page tables. Currently using splay tree for
    /// the storage
    /// @todo Consider AVL or Red-Black tree instead for better concurrency
    static page_table_map global_page_tables;
    static Spinlock page_table_index_lock;

    virtual void invalidate_range(kernel::paging::TLBShootdownContext &ctx, void *virt_addr,
                                  size_t size_bytes, bool free) override;

    x86_4level_Page_Table() = default;

    // Frees user pages
    void free_user_pages() override;

    void free_pt(u64 pt_phys);
    void free_pd(u64 pd_phys);
    void free_pdpt(u64 pdpt_phys);

    static void map_return_rec_nofree(const x86_PAE_Entry *entry_virt, u64 alignment_log, u64 start,
                                      u64 end, u64 curr_start);

    int flags = 0;

    kresult_t copy_to_recursive(const klib::shared_ptr<Page_Table> &to, u64 phys_page_level,
                                u64 absolute_start, u64 to_addr, u64 size_bytes, u64 new_access,
                                u64 current_copy_from, int level, u64 &upper_bound_offset,
                                kernel::paging::TLBShootdownContext &ctx);
};

const u16 rec_map_index = 509;

// Maps the pages. Returns the result
u64 map_pages(u64 physical_address, void *virtual_address, size_t size_bytes,
              kernel::paging::Page_Table_Arguments pta, u64 page_table_pointer);

u64 prepare_pt_for(void *virt_addr, kernel::paging::Page_Table_Arguments arg, u64 pt_top_phys);

// Releases cr3
extern "C" void release_cr3(u64 cr3);

}; // namespace kernel::x86::paging
