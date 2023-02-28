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

class Page_Table: public klib::enable_shared_from_this<Page_Table> {
public:
    PML4 *pml4_phys = nullptr;
    using pagind_regions_map = klib::splay_tree_map<u64, klib::unique_ptr<Generic_Mem_Region>>;
    pagind_regions_map paging_regions;
    klib::set<klib::weak_ptr<TaskDescriptor>> owner_tasks;
    u64 id = create_new_id();

    Spinlock lock;

    ~Page_Table();

    Page_Table(const Page_Table&) = delete;
    Page_Table(Page_Table&&) = delete;

    Page_Table& operator=(const Page_Table&) = delete;
    Page_Table& operator=(Page_Table&&) = delete;

    static u64 create_new_id()
    {
        static u64 top_id = 1;
        return __atomic_fetch_add(&top_id, 1, 0);
    }

    // Creates a page table structure from physical page table with 1 reference (during kernel initialization)
    static klib::shared_ptr<Page_Table> init_from_phys(u64 cr3);
    static klib::shared_ptr<Page_Table> create_empty_page_table();

    u64 get_cr3() const
    {
        return (u64)pml4_phys;
    }

    bool operator==(const Page_Table& p)
    {
        return pml4_phys == p.pml4_phys;
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
    u64 map(u64 page_addr, u64 virt_addr);
    u64 map(u64 page_addr, u64 virt_addr, Page_Table_Argumments arg);

    // Maps a page for a managed region, doing the appropriate checks
    u64 provide_managed(u64 page_addr, u64 virtual_addr);

    // Automatically invalidates a page table entry on all processors
    void invalidate_tlb(u64 page);

protected:
    void insert_global_page_tables();
    void takeout_global_page_tables();
    Page_Table() = default;

    u64 get_page_frame(u64 virt_addr);

    // Checks if the pages exists and invalidates it, invalidating TLB entries if needed
    void invalidate_nofree(u64 virt_addr);

    void unblock_tasks(u64 blocked_by_page);

    using page_table_map = klib::splay_tree_map<u64, klib::weak_ptr<Page_Table>>;
    static page_table_map global_page_tables;
    static Spinlock page_table_index_lock;

    // Gets the region for the page. Returns end() if no region exists
    pagind_regions_map::iterator get_region(u64 page);
};

const u16 rec_map_index = 256;

// Tries to assign a page. Returns result
u64 kernel_get_page(u64 virtual_addr, Page_Table_Argumments arg);
u64 kernel_get_page_zeroed(u64 virtual_addr, Page_Table_Argumments arg);

// Return true if mapped the page successfully
u64 map(u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg);

struct Check_Return_Str {
    kresult_t result = 0;
    u8 prev_flags = 0;
    bool allocated = 0;
};

Check_Return_Str check_if_allocated_and_set_flag(u64 virt_addr, u8 flag, Page_Table_Argumments arg);

// Invalidades a page entry
u64 invalidade(u64 virtual_addr);

// Constructs a new pml4
ReturnStr<u64> get_new_pml4();

// Release the page
kresult_t release_page_s(PTE& pte);

#define LAZY_FLAG_GROW_UP   0x01
#define LAZY_FLAG_GROW_DOWN 0x02

struct TaskDescriptor;

// Transfers pages from current process to process t
kresult_t atomic_transfer_pages(const klib::shared_ptr<TaskDescriptor>& from, const klib::shared_ptr<TaskDescriptor> to, u64 page_start, u64 to_addr, u64 nb_pages, Page_Table_Argumments pta);

// Prepares a page table for the address
kresult_t prepare_pt(u64 addr);

// Sets pte with pta at virtual_addr
kresult_t set_pte(u64 virtual_addr, PTE pte, Page_Table_Argumments pta);

// Checks if page is allocated
bool is_allocated(u64 page);

// Returns physical address of the virt
ReturnStr<u64> phys_addr_of(u64 virt);

// Invalidade a single page
void invalidade_noerr(u64 virtual_addr);

// Frees a page
void free_page(u64 page, u64 cr3);

// Frees a PML4 of a dead process
void free_pml4(u64 pml4, u64 pid);

// Releases cr3
extern "C" void release_cr3(u64 cr3);

// Frees user pages
void free_user_pages(u64 page_table);

// Prepares a user page for kernel reading or writing
kresult_t prepare_user_page(u64 page);

// Frees pages in range
void free_pages_range(u64 addr_start, u64 size, u64 cr3);

// Returns true if the address should not accessible to the user
inline bool is_in_kernel_space(u64 virt_addr)
{
    return virt_addr >= KERNEL_ADDR_SPACE;
}