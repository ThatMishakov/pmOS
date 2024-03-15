#pragma once
#include <memory/paging.hh>

/// @brief Indicates NX (no execute) bit is supported and enabled
/// @todo Very x86-specific
extern bool nx_bit_enabled;

/** @brief x86 paging structure, used in PAE, 4 level and 5 level paging
 *  
 * This structure is by the x86 CPUs to translate the physical addresses. Each table contains 512 entries and is page aligned. 
 * Unless accessing the last level of paging (Page Table itself) or using huge pages, entries point to the next levels of directories
 * untill the end is reached. A very good explanation in https://wiki.osdev.org/Paging
 */
struct x86_PAE_Entry {
    u8 present                   :1 = 0;
    u8 writeable                 :1 = 0;
    u8 user_access               :1 = 0;
    u8 write_through             :1 = 0;
    u8 cache_disabled            :1 = 0;
    u8 accessed                  :1 = 0;
    u8 dirty                     :1 = 0;
    u8 pat_size                  :1 = 0;
    u8 global                    :1 = 0; 
    u8 avl                       :3 = 0;  // Avaliable
    u64 page_ppn                 :40 = 0;
    u64 avl2                     :7 = 0;
    u8  pk                       :4 = 0;
    u8 execution_disabled        :1 = 0;

    /// Clears the entry (sets everything to 0, effectively invalidating it), freeing the page unless the no release flag is set.
    void clear_auto();

    /// Clears the entry without doing any checks
    void clear_nofree();
} PACKED ALIGNED(8);

class x86_Page_Table: public Page_Table {
public:
    virtual u64 get_cr3() const = 0;

    /// Automatically invalidates a page table entries, sending TLB shootdown IPI if needed
    void invalidate_tlb(u64 page, u64 size);

    /// True if the page table is used by the current task 
    bool is_active() const;

    /// True if the page table is used by other processors
    bool is_used_by_others() const;

    /// Atomically incerement and decrement active counter
    void atomic_active_sum(u64 val) noexcept;

    virtual void invalidate_tlb(u64 page);
protected:
    virtual u64 get_page_frame(u64 virt_addr) = 0;
    virtual void free_user_pages() = 0;

    volatile u64 active_count = 0;
};

class x86_4level_Page_Table: public x86_Page_Table {
public:
    x86_PAE_Entry *pml4_phys = nullptr;

    virtual u64 get_cr3() const override
    {
        return (u64)pml4_phys;
    }

    /**
     * @brief Creates an initial page table, capturing pointer to the root level
     * @param root Pointer to the root level of the page table
     * @return klib::shared_ptr<RISCV64_Page_Table> A shared pointer to the page table
     */
    static klib::shared_ptr<x86_4level_Page_Table> capture_initial(u64 root);

    static klib::shared_ptr<x86_4level_Page_Table> create_empty();

    // Maps the page with the appropriate permissions
    virtual void map(u64 page_addr, u64 virt_addr, Page_Table_Argumments arg) override;

    virtual void map(Page_Descriptor page, u64 virt_addr, Page_Table_Argumments arg) override;

    virtual void invalidate(u64 virt_addr, bool free) override;

    bool is_mapped(u64 virt_addr) const override;

    virtual ~x86_4level_Page_Table();

    constexpr u64 user_addr_max() const override
    {
        return 0x800000000000;
    }

    u64 phys_addr_of(u64 virt) const;

    static inline unsigned pt_index(u64 addr)
    {
        u64 index = (u64)addr >> 12;
        return index & 0777;
    }

    static inline unsigned pd_index(u64 addr)
    {
        u64 index = (u64)addr >> (12+9);
        return index & 0777;
    }

    static inline unsigned pdpt_index(u64 addr)
    {
        u64 index = (u64)addr >> (12+9+9);
        return index & 0777;
    }

    static inline unsigned pml4_index(u64 addr)
    {
        u64 index = (u64)addr >> (12+9+9+9);
        return index & 0777;  
    }

    Page_Info get_page_mapping(u64 virt_addr) const override;

    klib::shared_ptr<x86_4level_Page_Table> create_clone();

    virtual bool atomic_copy_to_user(u64 to, const void* from, u64 size);


    constexpr static u64 l4_align = 4096UL * 512 * 512 * 512;
    constexpr static u64 l3_align = 4096UL * 512 * 512;
    constexpr static u64 l2_align = 4096UL * 512;
    constexpr static u64 l1_align = 4096UL;

    static klib::shared_ptr<x86_4level_Page_Table> get_page_table_throw(u64 id);

    void apply() noexcept;
protected:
    /// @brief Inserts the page table into the map of the page tables
    static void insert_global_page_tables(klib::shared_ptr<x86_4level_Page_Table> table);

    /// @brief Takes out this page table from the map of the page tables
    void takeout_global_page_tables();

    using page_table_map = klib::splay_tree_map<u64, klib::weak_ptr<x86_4level_Page_Table>>;
    /// @brief Map holding all the page tables. Currently using splay tree for the storage
    /// @todo Consider AVL or Red-Black tree instead for better concurrency
    static page_table_map global_page_tables;
    static Spinlock page_table_index_lock;

    virtual void invalidate_range(u64 virt_addr, u64 size_bytes, bool free);

    virtual u64 get_page_frame(u64 virt_addr) override;
    x86_4level_Page_Table() = default;

    // Frees user pages
    void free_user_pages();

    void free_pt(u64 pt_phys);
    void free_pd(u64 pd_phys);
    void free_pdpt(u64 pdpt_phys);

    static void map_return_rec_nofree(const x86_PAE_Entry* entry_virt, u64 alignment_log, u64 start, u64 end, u64 curr_start);
};

const u16 rec_map_index = 509;

// Tries to assign a page using recursive mappings. Returns result
u64 kernel_get_page(u64 virtual_addr, Page_Table_Argumments arg);
u64 kernel_get_page_zeroed(u64 virtual_addr, Page_Table_Argumments arg);

// Return true if mapped the page successfully
void map(u64 cr3, u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg);

// Maps the pages. Returns the result
u64 map_pages(u64 physical_address, u64 virtual_address, u64 size_bytes, Page_Table_Argumments pta, u64 page_table_pointer);

u64 prepare_pt_for(u64 virt_addr, Page_Table_Argumments arg, u64 pt_top_phys);
u64 get_pt_ppn(u64 virt_addr, u64 pt_top_phys);

// Invalidades a page entry using recursive mappings
u64 invalidade(u64 virtual_addr);

struct TaskDescriptor;

// Invalidade a single page using recursive mappings
void invalidade_noerr(u64 virtual_addr);

// Releases cr3
extern "C" void release_cr3(u64 cr3);

/// Returns physical address of the virt using recursive mappings
ReturnStr<u64> phys_addr_of(u64 virt);
