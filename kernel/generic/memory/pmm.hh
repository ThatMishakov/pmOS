#pragma once
#include "intrusive_bst.hh"
#include "page_descriptor.hh"

#include <types.hh>

namespace kernel
{
namespace pmm
{

    /**
     * @brief Allocates count pages and returns the first one, containing the rest in a linked list
     *
     * @param count Number of pages to allocate
     * @return Page* Pointer to the first page
     */
    Page *alloc_pages(size_t count, bool contiguous = true) noexcept;

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
    phys_page_t get_memory_for_kernel(size_t number_of_pages) noexcept;

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

    struct PageArrayDescriptor {
        RBTreeNode<PageArrayDescriptor> by_address_tree;
        Page *pages;
        Page::page_addr_t start_addr;
        u64 size;
    };

    // Add a memory region to the PMM
    // Does not add it to the BST
    // Returns false on allocation failure
    bool add_page_array(Page::page_addr_t start_addr, u64 size, Page *pages) noexcept;

    Page *find_page(Page::page_addr_t addr) noexcept;

    // BST by pages pointer as a key
    using RegionsTree = RedBlackTree<
        PageArrayDescriptor, &PageArrayDescriptor::by_address_tree,
        detail::TreeCmp<PageArrayDescriptor, const Page *const, &PageArrayDescriptor::pages>>;

    extern RegionsTree::RBTreeHead memory_regions;

    // Sorted array of memory regions by start_addr
    extern PageArrayDescriptor *phys_memory_regions;
    extern size_t phys_memory_regions_count;
    extern size_t phys_memory_regions_capacity;

    using PageLL = CircularDoubleList<Page, &Page::free_region_list>;

    inline constexpr auto page_lists = 30 - PAGE_ORDER; // 1GB max allocation
    extern PageLL free_pages_list[page_lists];

    extern bool pmm_fully_initialized;

} // namespace pmm
} // namespace kernel