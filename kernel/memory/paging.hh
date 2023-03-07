#pragma once
#include <kernel/memory.h>
#include <types.hh>
#include <asm.hh>
#include <lib/pair.hh>
#include <lib/memory.hh>
#include <lib/set.hh>
#include <kernel/com.h>
#include "mem_regions.hh"
#include <lib/splay_tree_map.hh>

extern bool nx_bit_enabled;

#define PAGE_NORMAL  0
#define PAGE_SHARED  1
#define PAGE_SPECIAL 2
#define PAGE_DELAYED 3
#define PAGE_COW     4

struct Page_Table_Argumments {
    u8 writeable          : 1 = 0;
    u8 user_access        : 1 = 0;
    u8 global             : 1 = 0;
    u8 execution_disabled : 1 = 0;
    u8 extra              : 3 = 0;
};

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
} PACKED ALIGNED(8);

class Page_Table: public klib::enable_shared_from_this<Page_Table> {
public:
    using pagind_regions_map = klib::splay_tree_map<u64, klib::unique_ptr<Generic_Mem_Region>>;
    pagind_regions_map paging_regions;
    klib::set<klib::weak_ptr<TaskDescriptor>> owner_tasks;
    u64 id = create_new_id();

    Spinlock lock;

    virtual ~Page_Table();

    Page_Table(const Page_Table&) = delete;
    Page_Table(Page_Table&&) = delete;

    Page_Table& operator=(const Page_Table&) = delete;
    Page_Table& operator=(Page_Table&&) = delete;

    static u64 create_new_id()
    {
        static u64 top_id = 1;
        return __atomic_fetch_add(&top_id, 1, 0);
    }

    enum Protection {
        Readable = 0x01,
        Writeable = 0x02,
        Executable = 0x04,
    };

    enum Flags {
        Fixed = 0x01,
        Private = 0x02, 
        Shared = 0x04,
    };

    // Finds a spot for requested memory region
    ReturnStr<u64> find_region_spot(u64 desired_start, u64 size, bool fixed);

    ReturnStr<u64 /* page_start */> atomic_create_normal_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, u64 pattern);
    ReturnStr<u64 /* page_start */> atomic_create_managed_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, klib::shared_ptr<Port> t);
    ReturnStr<u64 /* page_start */> atomic_create_phys_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, u64 phys_addr_start);


    kresult_t prepare_user_page(u64 virt_addr, unsigned access_type, const klib::shared_ptr<TaskDescriptor>& task);
    kresult_t prepare_user_buffer(u64 virt_addr, unsigned access_type);

    bool can_takeout_page(u64 page_addr);

    // Gets a page table by its id or returns an empty pointer
    static klib::shared_ptr<Page_Table> get_page_table(u64 id);

    // Provides a page to a new page table
    static kresult_t atomic_provide_page(
            const klib::shared_ptr<TaskDescriptor>& from_task,
            const klib::shared_ptr<Page_Table>& to,
            u64 page_from,
            u64 page_to,
            u64 flags);

    // Maps the page with the appropriate permissions
    virtual u64 map(u64 page_addr, u64 virt_addr);
    virtual u64 map(u64 page_addr, u64 virt_addr, Page_Table_Argumments arg) = 0;

    struct Check_Return_Str {
        kresult_t result = 0;
        u8 prev_flags = 0;
        bool allocated = 0;
    };

    virtual Check_Return_Str check_if_allocated_and_set_flag(u64 virt_addr, u8 flag, Page_Table_Argumments arg) = 0;

    // Maps a page for a managed region, doing the appropriate checks
    virtual u64 provide_managed(u64 page_addr, u64 virtual_addr);

    // Checks whether page is allocated
    virtual bool is_allocated(u64 addr) const = 0;

    // Creates an empty page table, preparing appropriately the kernel entries
    // virtual klib::shared_ptr<Page_Table> create_empty_from_existing() = 0;

    // Maximum address that a pointer from the userspace can have
    virtual u64 user_addr_max() const = 0;

    // Returns true if the address should not accessible to the user
    inline bool is_in_kernel_space(u64 virt_addr)
    {
        return virt_addr >= user_addr_max();
    }

    // Returns physical address of the virt
    virtual ReturnStr<u64> phys_addr_of(u64 virt) const = 0;

    // Returns the physical limit
    static u64 phys_addr_limit();
protected:
    void insert_global_page_tables();
    void takeout_global_page_tables();
    Page_Table() = default;

    // Checks if the pages exists and invalidates it, invalidating TLB entries if needed
    virtual void invalidate_nofree(u64 virt_addr) = 0;
    void unblock_tasks(u64 blocked_by_page);

    using page_table_map = klib::splay_tree_map<u64, klib::weak_ptr<Page_Table>>;
    static page_table_map global_page_tables;
    static Spinlock page_table_index_lock;

    // Gets the region for the page. Returns end() if no region exists
    pagind_regions_map::iterator get_region(u64 page);

    // Gets a PPN from its virtual address
    virtual u64 get_page_frame(u64 virt_addr) = 0;
};

class x86_Page_Table: public Page_Table {
public:
    virtual u64 get_cr3() const = 0;

    // Automatically invalidates a page table entry on all processors
    void invalidate_tlb(u64 page);
protected:
    virtual u64 get_page_frame(u64 virt_addr) = 0;
    virtual void free_user_pages() = 0;
};

class x86_4level_Page_Table: public x86_Page_Table {
public:
    x86_PAE_Entry *pml4_phys = nullptr;

    virtual u64 get_cr3() const override
    {
        return (u64)pml4_phys;
    }

    // Creates a page table structure from physical page table with 1 reference (during kernel initialization)
    static klib::shared_ptr<x86_4level_Page_Table> init_from_phys(u64 cr3);

    static klib::shared_ptr<Page_Table> create_empty();

    // Maps the page with the appropriate permissions
    virtual u64 map(u64 page_addr, u64 virt_addr, Page_Table_Argumments arg) override;

    Check_Return_Str check_if_allocated_and_set_flag(u64 virt_addr, u8 flag, Page_Table_Argumments arg);

    virtual void invalidate_nofree(u64 virt_addr) override;

    bool is_allocated(u64 virt_addr) const override;

    // virtual u64 provide_managed(u64 page_addr, u64 virtual_addr) override;

    virtual ~x86_4level_Page_Table();

    constexpr u64 user_addr_max() const override
    {
        return 0x800000000000;
    }

    ReturnStr<u64> phys_addr_of(u64 virt) const;

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
protected:
    virtual u64 get_page_frame(u64 virt_addr) override;
    x86_4level_Page_Table() = default;

    // Frees user pages
    void free_user_pages();

    void free_pt(u64 pt_phys);
    void free_pd(u64 pd_phys);
    void free_pdpt(u64 pdpt_phys);
};

const u16 rec_map_index = 256;

// Tries to assign a page. Returns result
u64 kernel_get_page(u64 virtual_addr, Page_Table_Argumments arg);
u64 kernel_get_page_zeroed(u64 virtual_addr, Page_Table_Argumments arg);

// Return true if mapped the page successfully
u64 map(u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg);

PT* rec_prepare_pt_for(u64 virt_addr, Page_Table_Argumments arg);
u64 rec_get_pt_ppn(u64 virt_addr);

// Invalidades a page entry
u64 invalidade(u64 virtual_addr);

// Release the page
kresult_t release_page_s(x86_PAE_Entry& pte);

#define LAZY_FLAG_GROW_UP   0x01
#define LAZY_FLAG_GROW_DOWN 0x02

struct TaskDescriptor;

// Invalidade a single page
void invalidade_noerr(u64 virtual_addr);

// Frees a page
void free_page(u64 page, u64 cr3);

// Releases cr3
extern "C" void release_cr3(u64 cr3);

// Returns physical address of the virt
ReturnStr<u64> phys_addr_of(u64 virt);