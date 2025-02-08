#pragma once

#include "rcu.hh"

#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <types.hh>

class Mem_Object;

namespace kernel::pmm
{

using phys_page_t = u64;

struct Page {
    static void rcu_callback(void *self, bool chained) noexcept;
    Page() noexcept = default;

    using page_addr_t = u64;

    enum class PageType {
        Free = 0,
        PendingFree,
        Allocated,
        AllocatedPending, // Page has been allocated, but not mapped (or used) yet
        Reserved,         // First and last pages of regions
    };

    struct Mem_Object_LL_Head {
        Page *next;
        Mem_Object *owner;
        u64 offset;
        size_t refcount;
    };

    struct PendingAllocHead {
        Page *next;
        page_addr_t phys_addr;
        u64 size_pages;
    };

    struct FreeRegionHead {
        pmos::containers::DoubleListHead<Page> list;
        u64 size;
    };

    struct RCUState {
        RCU_Head rcu_h;
        size_t pages_to_free;
    };

    union {
        RCUState rcu_state;
        Mem_Object_LL_Head l;
        PendingAllocHead pending_alloc_head;
        pmos::containers::DoubleListHead<Page> free_region_list; // This is a bit of a workaround
        FreeRegionHead free_region_head;
    };

    PageType type = PageType::Free;
    int flags     = 0;

    static constexpr int FLAG_NO_PAGE   = 1 << 0;
    static constexpr int FLAG_ANONYMOUS = 1 << 1;

    bool has_physical_page() const noexcept { return !(flags & FLAG_NO_PAGE); }

    page_addr_t get_phys_addr() const noexcept;

    bool is_anonymous() const { return flags & FLAG_ANONYMOUS; }
};

void release_page(Page *) noexcept;

/// Structure describing a page. If it owns a page, the destructor automatically dealocates it
struct Page_Descriptor {
    Page *page_struct_ptr = nullptr;

    Page_Descriptor() noexcept = default;
    constexpr Page_Descriptor(Page *p) noexcept: page_struct_ptr(p) {};

    Page_Descriptor &operator=(Page_Descriptor &&) noexcept;
    Page_Descriptor(Page_Descriptor &&) noexcept;
    ~Page_Descriptor() noexcept;

    /// @brief Checks if the descriptor holds a page
    operator bool() const noexcept;

    /// Allocates a new page and copies current page into it
    ReturnStr<Page_Descriptor> create_copy() const noexcept;

    /// Creates another reference to the page
    Page_Descriptor duplicate() const noexcept;

    /// Frees the page that is held by the descriptor if it is managed
    void try_free_page() noexcept;

    /// @brief Allocates a page
    /// @param alignment_log Logarithm of the alignment of the page (e.g. 12 for 4096 bytes)
    /// @return Newly allocated page. Throws on error
    static ReturnStr<Page_Descriptor> allocate_page(u8 alignment_log);

    static ReturnStr<Page_Descriptor> allocate_page_zeroed(u8 alignment_log);

    /// Creates a page descriptor, allocating a new page struct, taking ownership of the given page
    static Page_Descriptor create_from_allocated(Page::page_addr_t phys_addr);

    /// Creates an empty page struct
    static Page_Descriptor create_empty() noexcept;

    /// Returns the descriptor with page_ptr == nullptr if the page was not found
    static Page_Descriptor find_page_struct(u64 page_base_phys) noexcept;

    /// Takes the page out of the descriptor, keeping its reference count
    /// Used in the page mapping functions
    Page::page_addr_t takeout_page() noexcept;

    /// Releases a page, that was previously taken out (mapped into a page table)
    void release_taken_out_page();

    /// Returns the physical address of the page pointer by this page descriptor
    Page::page_addr_t get_phys_addr() const noexcept;

    /// Creates a page descriptor from the pointer to the page, taking ownerhip of it
    static Page_Descriptor from_raw_ptr(Page *p) noexcept { return Page_Descriptor(p); }

    static Page_Descriptor dup_from_raw_ptr(Page *p) noexcept;

    static Page_Descriptor none() { return Page_Descriptor(nullptr); }
};

enum class AllocPolicy {
    Normal,
    Below4GB,
};

/**
 * @brief Allocates count pages and returns the first one, containing the rest in a linked list
 *
 * @param count Number of pages to allocate
 * @return Page* Pointer to the first page
 */
Page *alloc_pages(size_t count, bool contiguous = true, AllocPolicy policy = AllocPolicy::Normal) noexcept;

/**
 * @brief Gets the physical address of a page structure
 *
 * @param p Page structure
 * @return phys_page_t Physical address of the page
 */
phys_page_t phys_of_page(Page *p) noexcept;

inline bool alloc_failure(phys_page_t p) noexcept { return p == (phys_page_t)-1; }

/**
 * @brief Get the pages for the kernel (and marks it as such)
 *
 * This function can be used to allocate contiguous pages for the kernel, marking them as such.
 * The memory is contiguous, and this function is available since the early boot, before the PMM
 * is fully initialized.
 *
 * @return phys_page_t Physical address of the page. -1 if no page is available
 */
phys_page_t get_memory_for_kernel(size_t number_of_pages, AllocPolicy policy = AllocPolicy::Normal) noexcept;

/**
 * @brief Frees the contiguous pages allocated for the kernel
 *
 * @param page Physical address of the first page
 * @param number_of_pages Number of pages to free
 */
void free_memory_for_kernel(phys_page_t page, size_t number_of_pages) noexcept;

/**
 * Adds a page to the free list. It must be PendingFree or AllocatedPending. In the second case,
 * the whole chain is freed.
 *
 * @param p Pages to free
 */
void free_page(Page *p) noexcept;

using PageLL = pmos::containers::CircularDoubleList<Page, &Page::free_region_list>;

struct PMMRegion {
    Page::page_addr_t start;
    u64 size_bytes;

    constexpr PMMRegion(Page::page_addr_t start, u64 size_bytes) noexcept: start(start), size_bytes(size_bytes) {}

    inline static constexpr auto page_lists = 30 - PAGE_ORDER; // 1GB max allocation
    PageLL free_pages_list[page_lists];

    static PMMRegion *get(Page::page_addr_t start_addr);
};

struct PageArrayDescriptor {
    Page::page_addr_t start_addr;
    u64 size;
    PMMRegion *parent_region;
    pmos::containers::RBTreeNode<PageArrayDescriptor> by_address_tree;
    Page *pages;

    Page *end() const;

    Page::page_addr_t end_phys() const;

    static PageArrayDescriptor *find(Page::page_addr_t addr) noexcept;
    static PageArrayDescriptor *find(Page *p) noexcept;
    PageArrayDescriptor *next();
};

// Add a memory region to the PMM
// Does not add it to the BST
// Returns false on allocation failure
bool add_page_array(Page::page_addr_t start_addr, u64 size, Page *pages) noexcept;

Page *find_page(Page::page_addr_t addr) noexcept;

// BST by pages pointer as a key
using RegionsTree = pmos::containers::RedBlackTree<
    PageArrayDescriptor, &PageArrayDescriptor::by_address_tree,
    detail::TreeCmp<PageArrayDescriptor, const Page *const, &PageArrayDescriptor::pages>>;

extern RegionsTree::RBTreeHead memory_regions;

// Sorted array of memory regions by start_addr
extern PageArrayDescriptor *phys_memory_regions;
extern size_t phys_memory_regions_count;
extern size_t phys_memory_regions_capacity;

extern bool pmm_fully_initialized;

} // namespace kernel::pmm