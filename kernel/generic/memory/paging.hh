/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include "mem_protection.hh"
#include "mem_regions.hh"

#include <kernel/com.h>
#include <kernel/memory.h>
#include <lib/memory.hh>
#include <lib/pair.hh>
#include <lib/set.hh>
#include <lib/splay_tree_map.hh>
#include <pmos/containers/intrusive_list.hh>
#include <pmos/containers/set.hh>
#include <sched/sched.hh>
#include <sched/sched_queue.hh>
#include <types.hh>

/// TODO: I don't use this anymore but it still comes up in a couple of places and needs to be
/// removed
#define PAGE_NORMAL  0
#define PAGE_SHARED  1
#define PAGE_SPECIAL 2
#define PAGE_DELAYED 3
#define PAGE_COW     4

/// @brief Memory mapping type
enum class Memory_Type { Normal, MemoryNoCache, IONoCache };

/// @brief Arguments for mapping of pages
struct Page_Table_Argumments {
    u8 readable : 1           = 1;
    u8 writeable : 1          = 0;
    u8 user_access : 1        = 0;
    u8 global : 1             = 0;
    u8 execution_disabled : 1 = 0;
    u8 extra : 2              = 0;
    Memory_Type cache_policy  = Memory_Type::Normal;
};

class Mem_Object;
struct Mem_Object_Data {
    size_t handles_ref_count = 0;
    u8 max_privilege_mask = 0;
};

class Page_Table;

class TLBShootdownContext
{
public:
    using page_ptr = u64;
    struct range {
        page_ptr start;
        page_ptr size;
    };

    static TLBShootdownContext create_userspace(Page_Table &page_table);
    static TLBShootdownContext create_kernel();

    inline ~TLBShootdownContext() { finalize(); }
    void invalidate_range(page_ptr start, page_ptr size);
    void invalidate_page(page_ptr page);

    constexpr static short MAX_PAGES  = 512;
    constexpr static short MAX_RANGES = 16;

    bool flush_all() const;
    bool empty() const;
    bool for_kernel() const;

    template<auto TLBShootdownContext:: *array, short TLBShootdownContext:: *count> struct iter {
        TLBShootdownContext &ctx;
        inline auto begin() { return ctx.*array; }
        inline auto end() { return ctx.*array + ctx.*count; }
    };

    inline auto iterate_over_pages()
    {
        return iter<&TLBShootdownContext::pages, &TLBShootdownContext::pages_count> {*this};
    }

    inline auto iterate_over_ranges()
    {
        return iter<&TLBShootdownContext::ranges, &TLBShootdownContext::ranges_count> {*this};
    }

    void finalize();

private:
    Page_Table *page_table = nullptr;

    short pages_count  = 0;
    short ranges_count = 0;

    page_ptr pages[MAX_PAGES] = {};
    range ranges[MAX_RANGES]  = {};

    TLBShootdownContext() = default;

    inline const Page_Table *context_for() const { return page_table; }

    TLBShootdownContext(const TLBShootdownContext &) = delete;
    TLBShootdownContext(TLBShootdownContext &&)      = default;
};

struct Mem_Object_Reference;
struct Private_Normal_Region;

/// Generic struct, holding information about page mapping
struct Page_Info {
    u8 flags              = 0;
    bool is_allocated : 1 = 0;
    bool dirty : 1        = 0;
    bool user_access : 1  = 0;
    bool nofree : 1       = 0;
    bool writeable : 1    = 0;
    bool executable : 1   = 0;
    u64 page_addr         = 0;

    kernel::pmm::Page_Descriptor create_copy() const;

    kernel::pmm::Page *get_page() const;
};

/**
 * @brief Generic Page Table class
 *
 * This class contains a generic defenition of the page table and it is ment to built upon for
 * actual architecture-dependant storage and page table implementations and also for "virtual page
 * tables" used for sending memory regions inside the messages.
 */
class Page_Table: public klib::enable_shared_from_this<Page_Table>
{
public:
    /// Map containing user space memory regions. If the address is not covered by any of the
    /// regions, it is unallocated. Otherwise, the region controlls the actual allocation and
    /// protection of the memory.
    using RegionsRBTree = pmos::containers::RedBlackTree<
        Generic_Mem_Region, &Generic_Mem_Region::bst_head,
        detail::TreeCmp<Generic_Mem_Region, u64, &Generic_Mem_Region::start_addr>>;
    RegionsRBTree::RBTreeHead paging_regions;
    using Page_Info = ::Page_Info;

    /// List of the tasks that own the page table. Task_Descriptor should contain a page_table
    /// pointer which should point to this page table. The kernel does not have special structures
    /// for the threads, so they are achieved by a page table and using IPC for synchronization.
    pmos::containers::set<TaskDescriptor *> owner_tasks;

    /// Queue of the tasks blocked by the page table
    blocked_sched_queue blocked_tasks;

    /// Page Table ID. these are sequentially created upon the creation of new page tables (during
    /// the creation and initialization of the process or sending a message)
    u64 id = create_new_id();

    /// Lock for concurrent access. Each operation must lock it to protect the structures (splay
    /// trees) against nefarious userspace and concurrent pagefaults
    Spinlock lock;

    virtual ~Page_Table();

    Page_Table(const Page_Table &) = delete;
    Page_Table(Page_Table &&)      = delete;

    Page_Table &operator=(const Page_Table &) = delete;
    Page_Table &operator=(Page_Table &&)      = delete;

    /// Atomically creates a new id by increasing the top_id counter.
    static u64 create_new_id()
    {
        static u64 top_id = 1;
        return __atomic_fetch_add(&top_id, 1, 0);
    }

    /// ORable flags that can be used to indicate different protection levels.
    using Protection = ::Protection;

    constexpr static int FLAG_32BIT = 1;

    /// Flags used by regions properties and creation
    enum Flags {
        Fixed   = 0x01,
        Private = 0x02,
        Shared  = 0x04,
    };

    /**
     * @brief Finds a spot for requested memory region
     *
     * This function scans the page table and tries to find a suitable hole for allocating a new
     * memory region. Desired_start hint might be used to help the kernel with the region placement.
     * If it is not NULL and points to a space large enough to accomodate the new region, this
     * address should be returned. If, however, it is not large enough or the parameter is NULL, the
     * behaviour will depend upon the fixed variable. If it is set, the exception would be thrown.
     * Otherwise, the kernel shall walk the memory regions untill user_addr_max(). If a free spot
     * was found, then that address would be returned. Otherwise, an error is returned.
     *
     * @param desired_start Page-aligned hint that shall be used to place the new region. If
     * starting at *desired_addr* there are no regions which it might intersect it, that address
     * should be returned. Otherwise, either the new region should be found or an error should be
     * thrown, depending on the *fixed* parameter.
     * @param size Page-aligned size in bytes.
     * @param fixed Parameter indicating the behaviour in case the desired_start hint cannot be
     * honoured. If true, an exception would be thrown. If false, the new region should be found.
     * @return the beggining address where to place the new region.
     */
    ReturnStr<ulong> find_region_spot(ulong desired_start, ulong size,
                                            bool fixed);

    // Releases the memory regions in the given range (and their memory). If in the middle of the
    // region, it is split
    kresult_t release_in_range(TLBShootdownContext &ctx, u64 start, u64 size);
    kresult_t atomic_release_in_range(u64 start, u64 size);

    /// Moves the memory object to the new page table
    kresult_t atomic_transfer_object(const klib::shared_ptr<Page_Table> &new_table, u64 memory_object_id);

    /**
     * @brief Creates a normal memory region
     *
     * This function creates a memory region which lazilly allocates new pages when memory regions
     * are accessed. The newly allocated memory cannot be shared and is initialized to the
     * *pattern*.
     *
     * @param page_aligned_start A page-aligned hint indicating where to place the new region. This
     * hint is passed to find_region_spot() function, so see its behaviour to know how this is
     * honoured.
     * @param page_aligned_size A page-aligned size of the new region.
     * @param access A combination of bits from Protection enum, indicating the region memory
     * protection
     * @param fixed If the page_aligned_start should always be honoured. See find_region_spot().
     * @param name The name of the new region.
     * @param pattern The pattern that should be used to initialize to the newly allocated regions.
     * @return The start virtual address of the new memory region.
     * @see find_region_spot()
     */
    [[nodiscard]] ReturnStr<Mem_Object_Reference *> /* page_start */
        atomic_create_normal_region(ulong page_aligned_start, ulong page_aligned_size,
                                    unsigned access, bool fixed, bool dma, klib::string name,
                                    u64 pattern, bool cow);

    /**
     * @brief Creates a memory region mapping to the physical memory.
     *
     * This function creates a memory region which lazilly maps physical memory to the processe's
     * virtual memory. The kernel does not check if this memory is used or not and does not reserve
     * the pages automatically, so the caller should make sure other processes will not overwrite it
     * or do damage.
     *
     * @param page_aligned_start A page-aligned hint indicating where to place the new region. This
     * hint is passed to find_region_spot() function, so see its behaviour to know how this is
     * honoured.
     * @param page_aligned_size A page-aligned size of the new region.
     * @param access A combination of bits from Protection enum, indicating the region memory
     * protection
     * @param fixed If the page_aligned_start should always be honoured. See find_region_spot().
     * @param name The name of the new region.
     * @param phys_addr_start Page-aligned start of the physical address to which the virtual memory
     * should point to.
     * @return The start virtual address of the new memory region.
     * @see find_region_spot()
     */
    [[nodiscard]] ReturnStr<Phys_Mapped_Region *> /* page_start */
        atomic_create_phys_region(ulong page_aligned_start, ulong page_aligned_size,
                                  unsigned access, bool fixed, klib::string name,
                                  phys_addr_t phys_addr_start);

    /// Copies anonymous page at the given address
    [[nodiscard]] virtual kresult_t resolve_anonymous_page(u64 virt_addr, u64 access_type) = 0;

    /**
     * @brief Creates a memory region referencing the memory object
     *
     * This function creates a memory region, which would lazilly use pages from the memory object.
     * Depending on CoW parameter, the pages are either copied on access, or referenced, allowing
     * several processes to access shared memory. The memory object is pinned to the page table, so
     * it cannot be deleted until the page table is destroyed.
     *
     * @param page_aligned_start A page-aligned hint indicating where to place the new region. This
     * hint is passed to find_region_spot() function, so see its behaviour to know how this is
     * honoured.
     * @param page_aligned_size A page-aligned size of the new region.
     * @param access A combination of bits from Protection enum, indicating the region memory
     * protection
     * @param fixed If the page_aligned_start should always be honoured. See find_region_spot().
     * @param name The name of the new region.
     * @param object Memory object to be referenced by the new region.
     * @param cow If the memory should be copied on write or not.
     * @param start_offset_bytes Offset in bytes, after which the memory object is to be accessed.
     * Must be 0 for non-CoW regions
     * @param object_offset_bytes Offset in bytes of memory object to start_offset mapping. Must be
     * page-aligned for non-CoW regions
     * @param object_size_bytes Size of the memory object, after which the memory will be nulled. On
     * non-CoW regions, it must be equal to page_aligned_size.
     * @return The start virtual address of the new memory region.
     * @see find_region_spot()
     */
    [[nodiscard]] ReturnStr<Mem_Object_Reference *> atomic_create_mem_object_region(
        u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed,
        klib::string name, klib::shared_ptr<Mem_Object> object, bool cow, u64 start_offset_bytes,
        u64 object_offset_bytes, u64 object_size_bytes) noexcept;

    /**
     * @brief Prepares user page for being accessed by the kernel.
     *
     * This function prepares the user page to be accessed by the kernel in the mannes specified by
     * access_type. This function might block the task, in which case false will be returned.
     *
     * @param virt_addr Page-aligned virtual address of the page
     * @param access_type The OR combination of Protection enum entries signalling how the memory is
     * to be accessed.
     * @param task Task trying to access the memory. This function might block the task if deemed
     * necessary.
     * @return true The operation has been successfull and the page is immediately available.
     * @return false The operation has been successfull, however the page is not immediately
     * available and the task has been blocked. The action might need to be repeated once the page
     * becomes available and the task is unblocked.
     */
    [[nodiscard]] ReturnStr<bool> prepare_user_page(u64 virt_addr, unsigned access_type);
    // bool prepare_user_buffer(u64 virt_addr, unsigned access_type);

    /// @brief Indicates if the page can be taken out and used without copying to provide for the
    /// missing page
    /// @param page_addr Page aligned address
    /// @return true if the page can be taken out; false otherwise.
    bool can_takeout_page(u64 page_addr) noexcept;

    /// Gets a page table by its id or returns an empty pointer if no page table found
    static klib::shared_ptr<Page_Table> get_page_table(u64 id);

    /// Gets a page table by its id or throws an exception if no page table was found
    static klib::shared_ptr<Page_Table> get_page_table_throw(u64 id);

    /// Provides a page to a new page table
    /// @todo redo the function
    static bool atomic_provide_page(TaskDescriptor *from_task,
                                    const klib::shared_ptr<Page_Table> &to, u64 page_from,
                                    u64 page_to, u64 flags);

    /**
     * @brief Maps the page with the appropriate permissions
     *
     * This function deduces the appropriate permissions for the page by finding the corresponding
     * region and then maps it there, preparing the paging structures as needed (in case of using
     * multi-level page tables)
     *
     * @param page_addr Physical address of the page
     * @param virt_addr Virtual address to where the page shall be mapped
     * @return Error code
     */
    [[nodiscard]] virtual kresult_t map(u64 page_addr, u64 virt_addr) noexcept;

    /**
     * @brief Maps the page to the virtual address
     *
     * This function maps the specified page to the virtual address with given *arg* arguments and
     * adjusting and allocating memory for the appropriate paging structures as needed (in case of
     * using multi-level page tables)
     *
     * @param page_addr Physical address of the page
     * @param virt_addr Virtual address to where the page shall be mapped
     * @param arg Arguments and protections with which the page should be mapped.
     */
    [[nodiscard]] virtual kresult_t map(u64 page_addr, u64 virt_addr,
                                        Page_Table_Argumments arg) = 0;

    /**
     * @brief Maps the page to the virtual address
     *
     * This function maps the specified page to the virtual address with given *arg* arguments and
     * adjusting and allocating memory for the appropriate paging structures as needed (in case of
     * using multi-level page tables)
     *
     * @param page Descriptor of the page
     * @param virt_addr Virtual address to where the page shall be mapped
     * @param arg Arguments and protections with which the page should be mapped.
     */
    [[nodiscard]] virtual kresult_t map(kernel::pmm::Page_Descriptor page, u64 virt_addr,
                                        Page_Table_Argumments arg) noexcept = 0;

    // /// Return structure used with check_if_allocated_and_set_flag()
    // struct Check_Return_Str {
    //     u8 prev_flags = 0;
    //     bool allocated = 0;
    // };

    // /**
    //  * @brief Checks if the page is allocated and if it is not, sets a flag
    //  *
    //  * This function checks if the page is allocated. If it is, it signals that is it allocated
    //  and returns current flags. Otherwise, it sets the
    //  * flag in the *flag* parameter and returns the old flag.
    //  *
    //  * @param virt_addr Virtual address that is checked.
    //  * @param flag Flag to be set in case the page is not allocated
    //  * @param arg Arguments for the allocation of the new structures, in case multilevel pagins
    //  structures are in use
    //  * @return Returns old flag value and whether the page is allocated.
    //  */
    // virtual Check_Return_Str check_if_allocated_and_set_flag(u64 virt_addr, u8 flag,
    // Page_Table_Argumments arg) = 0;

    /// @brief Checks whether page is mapped
    ///
    /// This function checks whether the page is mapped, without checking the allocation regions.
    /// @returns true if the page is mapped.
    /// @returns false if the page is not mapped.
    virtual bool is_mapped(u64 addr) const = 0;

    // Creates an empty page table, preparing appropriately the kernel entries
    // virtual klib::shared_ptr<Page_Table> create_empty_from_existing() = 0;

    /// Maximum address that a pointer from the userspace can have in a given page table
    virtual u64 user_addr_max() const = 0;

    /// Returns true if the address should not accessible to the user
    inline bool is_in_kernel_space(u64 virt_addr) { return virt_addr >= user_addr_max(); }

    /// @brief Returns the limit of the physical address that is supported by the given paging
    /// scheme
    /// @return maximum value (first not supported) that can be a valid physical address
    static u64 phys_addr_limit();

    /**
     * @brief Atomically takes out the paging region and transfers it to a new page table
     *
     * Atomically takes out the paging region of the current page table and transfers it to the new
     * one using new access flag, finding a spot for it. The preferred_to is used as a hint. If
     * possible, the region would be placed there, otherwise, depending on the fixed attribute,
     * either a new region would be found or an exception would be thrown.
     *
     * @param to The page table to which the region shall be transfered
     * @param region_orig The start of the region in the old page table to identify the region that
     * will be transfered
     * @param prefered_to The prefered location where the new region is to be placed. If it cannot
     * be placed there, the behaviour is dependent on the fixed parameter.
     * @param access The protections that the region should have in the new page table.
     * @param fixed If true, if the region cannot be placed where the preferred_to points to, the
     * exception will be thrown. Otherwise, the new region would be found.
     * @return The location of the region in the new page table.
     */
    ReturnStr<size_t> atomic_transfer_region(const klib::shared_ptr<Page_Table> &to,
                                             size_t region_orig, size_t prefered_to, ulong access,
                                             bool fixed);

    /**
     * @brief  Moves the mapped pages from the old region to a new region, invaludating the old page
     * table as needed
     *
     * @param to Page Table to which the pages shall be moved
     * @param from_addr From where the pages should be taken out in the old tage table
     * @param to_addr To where the pages should be transferred in the new page table
     * @param size_bytes Size of the region that should be transferred in bytes
     * @param new_access The protections that the pages should have after being moved to the new
     * page table
     */
    [[nodiscard]] kresult_t move_pages(TLBShootdownContext &ctx,
                                       const klib::shared_ptr<Page_Table> &to, size_t from_addr,
                                       size_t to_addr, size_t size_bytes, u8 new_access) noexcept;

    /**
     * @brief  Copies (installs) anonymous pages from the old page table to the new page table.
     *
     * This function maps the anonymous pages as read only, and shares them with the new page table,
     * achieving the copy-on-write semantics.
     *
     * @param to Page Table to which the pages shall be copied
     * @param from_addr From where the pages should be taken out in the old tage table
     * @param to_addr To where the pages should be transferred in the new page table
     * @param size_bytes Size of the region that should be transferred in bytes
     * @param new_access The protections that the pages should have after being moved to the new
     * page table
     */
    virtual kresult_t copy_anonymous_pages(const klib::shared_ptr<Page_Table> &to, u64 from_addr,
                                           u64 to_addr, u64 size_bytes, u8 new_access) = 0;

    /// Gets information for the page mapping.
    virtual Page_Info get_page_mapping(u64 virt_addr) const = 0;

    /**
     * @brief Pin a memory object to the page table
     *
     * This function pins a reference (pointer) to the memory object to this page table. If the
     * object was already pinned, nothing should be done.
     *
     * @param object A valid pointer to the object to be pinned.
     */
    kresult_t atomic_pin_memory_object(klib::shared_ptr<Mem_Object> object);

    /**
     * @brief Remove a reference to the memory object from the page table
     *
     * @param object Object to be removed
     */
    kresult_t atomic_unpin_memory_object(klib::shared_ptr<Mem_Object> object);

    /**
     * @brief Shrink memory regions referencing *id*
     *
     * This procedure cheks all the memory regions if they go out of bound of the new size and
     * shrinks the memory regions that are larger than it accordingly
     *
     * @param id Memory region that is shrunk
     * @param new_size New size of the region in bytes
     */
    void atomic_shrink_regions(const klib::shared_ptr<Mem_Object> &id, u64 new_size) noexcept;

    /**
     * @brief Deletes a memory region identified by (starting at) *region_start*
     *
     * @param region_start Start of the memory region
     *
     * If the region exists, it is automatically deleted and the underlying memory is freed. If it
     * is not, an error is thrown
     */
    kresult_t atomic_delete_region(u64 region_start);

    /**
     * @brief Unreferences memory region from memory object
     *
     * @param object Memory object associated with the region
     * @param region Memory region to be deleted
     */
    void unreference_object(const klib::shared_ptr<Mem_Object> &object,
                            Mem_Object_Reference *region) noexcept;

    /**
     * @brief Invalidates the TLB entries for the given page in the given page table
     *
     * @param page Page to be invalidated
     */
    virtual void invalidate_tlb(u64 page)            = 0;
    virtual void invalidate_tlb(u64 start, u64 size) = 0;
    virtual void tlb_flush_all()                     = 0;

    /**
     * @brief Copies the data from kernel to the user space
     *
     * This function copies data from kernel to user space at the indicated address. Is does not
     * have to be of an active page table.
     *
     * @param to Virtual address in the user space
     * @param from Pointer to the data in the kernel space
     * @param size Size of the data in bytes
     * @return true The operation has been successfull
     * @return false The pages are not immediately available, and the task should be blocked and the
     * operation should be repeated once the pages are available.
     * @throw Kern_Exception If the operation is not successfull
     */
    virtual ReturnStr<bool> atomic_copy_to_user(u64 to, const void *from, u64 size);

    /// @brief Checks if the pages exists and invalidates it, invalidating TLB entries if needed
    /// @param virt_addr Virtual address of the page
    /// @param free Indicates whether the page should be freed or not after invalidating
    virtual void invalidate(TLBShootdownContext &ctx, u64 virt_addr, bool free) = 0;

    /// @brief Invalidates the pages in the given range, also invalidating TLB entries as needed.
    /// @param virt_addr Virtual address of the start of the region that should be invalidated
    /// @param size_bytes Size of the regions in bytes
    /// @param free Indicates whether the pages should be freed after being invalidated
    virtual void invalidate_range(TLBShootdownContext &ctx, u64 virt_addr, u64 size_bytes,
                                  bool free) = 0;

    void /* generation */ apply_cpu(CPU_Info *cpu);
    void unapply_cpu(CPU_Info *cpu);

    // TODO: Calling this on page table is weird
    void trigger_shootdown(CPU_Info *cpu);

protected:
    Page_Table() = default;

    /// @brief Unblocks the tasks blocked by the given page
    /// @param blocked_by_page Virtuall address of the page that has blocked the tasks
    void unblock_tasks(u64 blocked_by_page);
    void unblock_tasks_rage(u64 blocked_by_page, u64 size_bytes);

    /// Gets the region for the page. Returns end() if no region exists
    RegionsRBTree::RBTreeIterator get_region(u64 page);

    /// Storage for the pointers to the pinned memory objects
    klib::splay_tree_map<klib::shared_ptr<Mem_Object>, Mem_Object_Data> mem_objects;

    using objects_set = pmos::containers::RedBlackTree<
        Mem_Object_Reference, &Mem_Object_Reference::object_bst_head,
        detail::TreeCmp<Mem_Object_Reference, u64, &Mem_Object_Reference::start_addr>>::RBTreeHead;

    /// Storage for regions for memory objects
    klib::splay_tree_map<Mem_Object *, objects_set> object_regions;

    // TODO: This is not good
    friend struct Mem_Object_Reference;

    CriticalSpinlock active_cpus_lock;
    int paging_generation    = 0;
    int active_cpus_count[2] = {0, 0};
    using list =
        pmos::containers::InitializedCircularDoubleList<CPU_Info, &CPU_Info::active_page_table>;
    list active_cpus[2]                       = {};
    TLBShootdownContext *shootdown_descriptor = nullptr;

    friend class TLBShootdownContext;
};

extern int kernel_pt_generation;

// Arch-generic pointer to the physical address of the top-level page table
using ptable_top_ptr_t = u64;

// Generic function to map a page
kresult_t map_page(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr,
                   Page_Table_Argumments arg);

// Generic functions to map and release pages in kernel, using the active page table
kresult_t map_kernel_page(u64 phys_addr, void *virt_addr, Page_Table_Argumments arg);
kresult_t unmap_kernel_page(TLBShootdownContext &ctx, void *virt_addr);

// Generic function to map multiple pages
kresult_t map_pages(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr, u64 size_bytes,
                    Page_Table_Argumments arg);
kresult_t map_kernel_pages(u64 phys_addr, u64 virt_addr, u64 size, Page_Table_Argumments arg);

// Generic function to apply the page table to the current CPU
void apply_page_table(ptable_top_ptr_t page_table);

// Functions to be defined by the architecture to invalidate the TLB entries for kernel pages
// (i.e. those that have global bit set on x86)
void invalidate_tlb_kernel(u64 start);
void invalidate_tlb_kernel(u64 start, u64 size);