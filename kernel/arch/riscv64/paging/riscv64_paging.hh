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
#include <types.hh>

#define PBMT_PMA 0x0
#define PBMT_NC  0x1
#define PBMT_IO  0x2

struct RISCV64_PTE {
    bool valid : 1 = false; // Indicates if the PTE (page table entry) is valid.
                            // If not set, the other bits are ignored and can be
                            // used for other purposes by the OS

    bool readable : 1   = false; // RWE bits define permissions. Set to 0b000 they
    bool writeable : 1  = false; // are an indication that PTE points
    bool executable : 1 = false; // to a leaf PTE. Otherwise, if not a leaf
                                 // table has them, this is a huge page.

    bool user : 1 = false; // Specifies if the page is accessible by user mode
                           // Note: Risk-V requires the kernel to set the SUM
                           // bit in the sstatus register to allow accessing
                           // user mode pages while in kernel

    bool global : 1 = false; // If set, the page is global and survives
                             // TLB flushes. If the top level PTE has it set,
                             // the leaf entries are also treated as global.

    bool accessed : 1 = false; // Indicates that the page has been read, written
                               // or fetched since the bit was clear

    bool dirty : 1 = false; // Indicates that the page has been written since
                            // the bit was clear

    u8 available : 2 = 0; // Available for software use
                          // pmOS uses them to indicate the physical mapping
                          // pages and shared pages

    u64 ppn : 44 = 0; // Physical page number. Note: On x86, it starts
                      // at 12th bit, on RISC-V it's the 10th. Can be
                      // calculated by shifting the start of the physical
                      // address to the right by 12 bits

    u16 reserved : 7 = 0;     // Reserved for future use. Must be 0
    u8 pbmt : 2      = 0;     // Page-based memory type. 0 if unused
    bool napot : 1   = false; // Svnapot extension stuff. Must be zero if unused

    inline bool is_leaf() const noexcept { return readable or writeable or executable; }

    inline bool is_special() const noexcept { return available != 0; }

    /// Clears the entry, freeing the memory if needed
    void clear_auto();
} __attribute__((packed, aligned(8)));

class TLBShootdownContext;

// Active number of paging levels of the system, e.g. 4 for SV48.
// At this moment, 4 is hardcoded, but this can be adjusted during the boot
// tu support SV57 or SV39 on systems with little memory
inline u8 riscv64_paging_levels = 4;

// Flushes the given page from the TLB cache
// This is a pinpoint operation, and only takes care of the hart it is ran on.
// If the page could be used by other CPUs, an IPI needs to be sent...
void flush_page(void *virt_addr) noexcept;

// Prepares a leaf PT for a given virtual address. This function is used during
// temp mapper initialization.
ReturnStr<u64> prepare_leaf_pt_for(void *virt_addr, Page_Table_Argumments arg, u64 pt_ptr);

// Maps a page to a given virtual address, using the available temporary maper.
// riscv64_paging_levels is used to determine the number of levels of the page
// table This function can allocate pages for leaf entries, if not already
// installed
kresult_t riscv_map_page(u64 pt_top_phys, void *virt_addr, u64 phys_addr, Page_Table_Argumments arg);

// Unmaps the page from the given virtual address, using the available temporary
// maper. If the page is not special, it's freed
kresult_t riscv_unmap_page(TLBShootdownContext &ctx, u64 pt_top_phys, void *virt_addr);

// Gets the top level page table pointer for the current hart
u64 get_current_hart_pt() noexcept;

/* final allow virtual functions optimizations */
class RISCV64_Page_Table final: public Page_Table
{
public:
    // Gets the page table by its ID
    static klib::shared_ptr<RISCV64_Page_Table> get_page_table(u64 id) noexcept;

    /**
     * @brief Creates an initial page table, capturing pointer to the root level
     * @param root Pointer to the root level of the page table
     * @return klib::shared_ptr<RISCV64_Page_Table> A shared pointer to the page
     * table
     */
    static klib::shared_ptr<RISCV64_Page_Table> capture_initial(u64 root);

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
     * @return klib::shared_ptr<RISCV64_Page_Table> A clone of the page table
     */
    klib::shared_ptr<RISCV64_Page_Table> create_clone();

    /**
     * @brief Creates an empty page table
     *
     * This function creates a new empty page table. The correct kernel tables
     * are inherited, and the userspace has empty address space with no mappings
     * and memory regions.
     *
     * @return klib::shared_ptr<RISCV64_Page_Table> A pointer to the new page
     * table
     */
    static klib::shared_ptr<RISCV64_Page_Table> create_empty(int flags = 0);

    /// @brief Atomically add to the active_counter
    ///
    /// This function should atomically add the given integer to the
    /// active_counter variable This is typically done in scheduling on task
    /// switches, helping to track how many tasks are there to do TLB shootdowns
    /// @param i Integer to be added
    void atomic_active_sum(u64 i) noexcept;

    /// Installs the page table
    void apply() noexcept;

    /// Gets the root of the page table
    u64 get_root() const noexcept { return table_root; }

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

    /// Checks if the page is mapped
    bool is_mapped(void *virt_addr) const noexcept override;

    Page_Info get_page_mapping(void *virt_addr) const override;

    virtual void invalidate_range(TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes, bool free) override;

    virtual kresult_t map(u64 page_addr, void *virt_addr, Page_Table_Argumments arg) override;
    virtual kresult_t map(kernel::pmm::Page_Descriptor page, void *virt_addr, Page_Table_Argumments arg) override;

    kresult_t resolve_anonymous_page(void *virt_addr, unsigned access_type) override;

    virtual ~RISCV64_Page_Table() override;

    // Clears the TLB cache for the given page
    void invalidate_tlb(void *page) override;
    void invalidate_tlb(void *start, size_t size) override;
    void tlb_flush_all() override;

    ReturnStr<bool> atomic_copy_to_user(void *to, const void *from, size_t size) override;

    virtual kresult_t copy_anonymous_pages(const klib::shared_ptr<Page_Table> &to, void *from_addr, void *to_addr,
                    size_t size_bytes, unsigned new_access) override;

    bool is_32bit() const noexcept { return false; }
protected:
    /// Root node/top level of paging structures
    u64 table_root = 0;

    RISCV64_Page_Table() = default;

    static kresult_t copy_to_recursive(const klib::shared_ptr<Page_Table> &to, u64 phys_page_level, u64 start, u64 to_offset, u64 max_size, u64 new_access, u64 i, int level, u64 &last_copied, TLBShootdownContext &ctx);

private:
    // There is an interesting pattern for this:
    // https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern#Object_counter
    // But it seems to be very clunky with shared pointers, so I'm not using it

    /// @brief Inserts the page table into the map of the page tables
    static kresult_t insert_global_page_tables(klib::shared_ptr<RISCV64_Page_Table> table);

    /// @brief Takes out this page table from the map of the page tables
    void takeout_global_page_tables();

    using page_table_map = klib::splay_tree_map<u64, klib::weak_ptr<RISCV64_Page_Table>>;
    /// @brief Map holding all the page tables. Currently using splay tree for
    /// the storage
    /// @todo Consider AVL or Red-Black tree instead for better concurrency
    static page_table_map global_page_tables;
    static Spinlock page_table_index_lock;

    /// Counter of how many Harts have this page table active
    u64 active_counter = 0;

    /// Frees user space pages. Should be called when deleting the page table
    /// pointer
    void free_user_pages();
};