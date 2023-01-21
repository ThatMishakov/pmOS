#pragma once
#include <kernel/memory.h>
#include <types.hh>
#include <asm.hh>
#include <lib/pair.hh>
#include <lib/memory.hh>
#include <kernel/com.h>

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

struct Page_Table {
    PML4 *pml4_phys = nullptr;
    struct shared_info {
        u64 refcount = 1;
        Spinlock lock;
    } *shared_str = nullptr;

    kresult_t prepare_new(const Page_Table*);
    Page_Table() = default;
    Page_Table(const Page_Table&) noexcept;
    Page_Table(Page_Table&&) noexcept;
    ~Page_Table();

    Page_Table& operator=(const Page_Table&) noexcept;
    Page_Table& operator=(Page_Table&&) noexcept;

    // Creates a page table structure from physical page table with 1 reference (during kernel initialization)
    static Page_Table init_from_phys(u64 cr3);
    static Page_Table get_new_page_table();

    u64 get_cr3() const
    {
        return (u64)pml4_phys;
    }

    bool operator==(const Page_Table& p)
    {
        return pml4_phys != p.pml4_phys;
    }
};

const u16 rec_map_index = 511;

// Tries to assign a page. Returns result
u64 get_page(u64 virtual_addr, Page_Table_Argumments arg);
u64 get_page_zeroed(u64 virtual_addr, Page_Table_Argumments arg);

// Return true if mapped the page successfully
u64 map(u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg);

// Invalidades a page entry
u64 invalidade(u64 virtual_addr);

// Constructs a new pml4
ReturnStr<u64> get_new_pml4();

// Release the page
kresult_t release_page_s(u64 virtual_address, u64 pid);

// Preallocates an empty page (to be sorted out later by the pagefault manager)
kresult_t alloc_page_lazy(u64 virtual_addr, Page_Table_Argumments arg, u64 flags);

kresult_t get_lazy_page(u64 virtual_addr);

#define LAZY_FLAG_GROW_UP   0x01
#define LAZY_FLAG_GROW_DOWN 0x02

struct TaskDescriptor;

// Transfers pages from current process to process t
kresult_t atomic_transfer_pages(const klib::shared_ptr<TaskDescriptor>& from, const klib::shared_ptr<TaskDescriptor> to, u64 page_start, u64 to_addr, u64 nb_pages, Page_Table_Argumments pta);

// Sharess pages from current process with process t
kresult_t atomic_share_pages(const klib::shared_ptr<TaskDescriptor>& t, u64 page_start, u64 to_addr, u64 nb_pages, Page_Table_Argumments pta);

// Prepares a page table for the address
kresult_t prepare_pt(u64 addr);

// Sets pte with pta at virtual_addr
kresult_t set_pte(u64 virtual_addr, PTE pte, Page_Table_Argumments pta);

// Checks if page is allocated
bool is_allocated(u64 page);

// Returns physical address of the virt
ReturnStr<u64> phys_addr_of(u64 virt);

enum Page_Types {
    NORMAL,
    HUGE_2M,
    HUGE_1G,
    UNALLOCATED,
    LAZY_ALLOC,
    SHARED,
    COW,
    SPECIAL,
    UNKNOWN
};

// Returns page type
Page_Types page_type(u64 virtual_addr);

// Invalidade a single page
void invalidade_noerr(u64 virtual_addr);

// Frees a page
void free_page(u64 page, u64 pid);

// Frees a PML4 of a dead process
void free_pml4(u64 pml4, u64 pid);

// Releases cr3
extern "C" void release_cr3(u64 cr3);

// Frees user pages
void free_user_pages(u64 page_table);

// Prepares a user page for kernel reading or writing
kresult_t prepare_user_page(u64 page);

// Makes the page shared and returns its PTE
ReturnStr<klib::pair<PTE, bool>> share_page(u64 virtual_addr, u64 owner_page_table);

// Unshares a page and makes PID its only owner
kresult_t unshare_page(u64 virtual_addr, u64 pid);

// Returns true if the address should not accessible to the user
inline bool is_in_kernel_space(u64 virt_addr)
{
    return virt_addr >= KERNEL_ADDR_SPACE;
}