#pragma once
#include <types.hh>
#include <lib/memory.hh>
#include <lib/string.hh>
#include "page_descriptor.hh"

class Port;
class TaskDescriptor;
class Page_Table;

extern u64 counter;
struct Page_Table_Argumments;

/** 
 * @brief Generic memory region class. Defines an action that needs to be executed when pagefault is produced.
 * 
 * This is a polymorphic base for memory regions, which describe different ways of how kernel should map its memory and
 * deal with pagefaults and whatnot. These regions are stored a splay tree in the parent Page_Table class.
 * 
 * In the "simple" region, the kernel might "simply" allocate the empty pages or map physical memory to the process, however
 * in a more complex situations, this task is pushed onto the userspace pagers/memory managers/programs which like to interract with kernel too much
 * for no aparent reason, depending on how the region is configured.
 */
struct Generic_Mem_Region: public klib::enable_shared_from_this<Generic_Mem_Region> {
    virtual ~Generic_Mem_Region() = default;

    /// Asigns the ID sequentially upon region creation.
    Generic_Mem_Region(): id(__atomic_add_fetch(&counter, 1, 0))
        {}

    /// Page aligned start of the region
    u64 start_addr = 0;

    /// Page aligned size
    u64 size = 0;

    /// Optional name of the region
    klib::string name;

    /// Unique ID which identifies the region
    u64 id = 0;

    /// Owner of the region
    Page_Table *owner;

    /// Flags for the region
    u8 access_type = 0;

    static constexpr u8 Readable   = 0x01;
    static constexpr u8 Writeable  = 0x02;
    static constexpr u8 Executable = 0x04;

    /**
     * @brief Allocate a page inside the region
     * 
     * This function tries to allocate the page for the task (as the kernel is *very* lazy (which is a good thing) and uses delayed allocation for everything).
     * Depending on the actual region class, different actions could be done, one of them being mercilessly blocking the task desperately trying to access it.
     * 
     * @param ptr_addr The address which needs to be allocated/has caused the petition.
     * @return true if the page is avaiable, false otherwise.
     */
    virtual bool alloc_page(u64 ptr_addr) = 0;

    /**
     * @brief Checks if a page from the region can be taken out by provide_page() syscall
     */
    constexpr virtual bool can_takeout_page() const noexcept
    {
        return false;
    }

    /**
     * @brief Prepares the page for being accessed by the kernel.
     * 
     * This function prepares the page to be accessed by the kernel by checking permissions and whatnot and calling alloc_page() with the specially-crafter
     * parameters. See alloc_page() for more details.
     * 
     * @param access_mode Mode in which the page is to be accessed
     * @param page_addr Address of the page
     * @return true if the execution was successfull and the page is immediately available
     * @return false is the execution was successfull but the resource is currently unavailable and the operation is needed to be repeated again once
     *         it becomes available.
     * @see alloc_page()
     */
    bool prepare_page(u64 access_mode, u64 page_addr);

    /**
     * @brief Function to be executed upon a page fault.
     * 
     * @param error Pagefault error (passed with the exception)
     * @param pagefault_addr Address of the pagefault
     * @param task Task causing the pagefault
     * @return true Execution was successfull and the page is immediately available
     * @return false Execution was successfull but the page is not immediately available
     * @todo This function is very x86-specific
     */
    bool on_page_fault(u64 error, u64 pagefault_addr);


    Generic_Mem_Region(u64 start_addr, u64 size, klib::string name, Page_Table *owner, u8 access):
        start_addr(start_addr), size(size), name(klib::forward<klib::string>(name)), id(__atomic_add_fetch(&counter, 1, 0)), owner(owner), access_type(access)
        {};

    /**
     * @brief Checks if the address is inside the memory region
     * 
     * @param addr Memory address
     * @return true It is inside the region
     * @return false It is outside the region
     */
    bool is_in_range(u64 addr) const
    {
        return addr >= start_addr and addr < start_addr+size;
    }

    /**
     * @brief Checks if the process has permissions to access the region
     * 
     * @param err_code Error code produced by a pagefault
     * @return true Process has permission
     * @return false Process has no permisison
     */
    bool has_permissions(u64 err_code) const noexcept
    {
        return (not ((err_code & 0x02) and not (access_type & Writeable))) and (not ((err_code & 0x10) and not (access_type & Executable)));
    }

    bool has_access(unsigned access_mask) const noexcept
    {
        return not (access_mask & ~this->access_type);
    }

    static inline bool protection_violation(u64 err_code) noexcept
    {
        return err_code & 0x01;
    }

    u64 addr_end() const noexcept
    {
        return start_addr + size;
    }

    /// @brief Prepares the appropriate Page_Table_Arguments for the region
    virtual Page_Table_Argumments craft_arguments() const = 0;

    constexpr virtual bool is_managed() const noexcept
    {
        return false;
    }

    /**
     * @brief Moves the region to the new page table
     * 
     * @param new_table The page table to which the region is to be moved
     * @param base_addr Base address in the new page table 
     * @param new_access New access specifier to be had in the new page table
     */
    virtual void move_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access);

    /**
     * @brief Clones the region to the new page table
     * 
     * @param new_table The page table to which the region is to be cloned
     * @param base_addr Base address in the new page table
     * @param new_access New access specifier to be had in the new page table
     */
    virtual void clone_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access) = 0;

    /**
     * @brief Prepares a region for being deleted
     * 
     * This procedure must be called before deleting the memory region. It does not free pages and that must be done
     * by the caller after calling the function.
     * 
     */
    virtual void prepare_deletion() noexcept;
};


/**
 * @brief Physical mapping region class.
 * 
 * This region is very simple and just maps the physical pages to the virtual address space. Invaluable for drivers
 * since we are a microkernel.
 * 
 * @see syscall_create_phys_map_region()
 */
struct Phys_Mapped_Region final: Generic_Mem_Region {
    // Allocated a new page, pointing to the corresponding physical address.
    virtual bool alloc_page(u64 ptr_addr);

    u64 phys_addr_start = 0;
    constexpr bool can_takeout_page() const noexcept override
    {
        return false;
    }

    virtual void clone_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access) override;

    virtual Page_Table_Argumments craft_arguments() const;

    // Constructs a region with virtual address starting at *aligned_virt* of size *size* pointing to *aligned_phys*
    Phys_Mapped_Region(u64 aligned_virt, u64 size, u64 aligned_phys);

    Phys_Mapped_Region(u64 start_addr, u64 size, klib::string name, Page_Table * owner, u8 access, u64 phys_addr_start):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner, access), phys_addr_start(phys_addr_start)
        {};
};


// Normal region. On page fault, attempts to allocate the page and fills it with specified pattern (e.g. zeroing) if successfull
struct Private_Normal_Region final: Generic_Mem_Region {
    // This pattern is used to fill the newly allocated pages
    u64 pattern = 0;

    constexpr bool can_takeout_page() const noexcept override
    {
        return true;
    }

    virtual void clone_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access) override;

    // Attempt to allocate a new page
    virtual bool alloc_page(u64 ptr_addr);

    // Tries to preallocate all the pages
    void prefill();

    virtual Page_Table_Argumments craft_arguments() const;

    Private_Normal_Region(u64 start_addr, u64 size, klib::string name, Page_Table * owner, u8 access, u64 pattern):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner, access), pattern(pattern)
        {};
};

class Mem_Object;

/// Memory region which references memory object
struct Mem_Object_Reference final : Generic_Mem_Region
{
    /// Memory object that is referenced by the region
    const klib::shared_ptr<Mem_Object> references;

    /// Offset in bytes from the start of the memory object
    u64 start_offset_bytes = 0;

    /// Indicates whether the pages should be copied on access
    bool cow = false;

    Mem_Object_Reference(u64 start_addr, u64 size, klib::string name, Page_Table *owner, u8 access, klib::shared_ptr<Mem_Object> references, u64 start_offset_bytes, bool copy_on_write):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner, access), 
        references(klib::forward<klib::shared_ptr<Mem_Object>>(references)), start_offset_bytes(start_offset_bytes), cow(copy_on_write)
        {};

    virtual bool alloc_page(u64 ptr_addr) override;

    virtual void move_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access) override;
    virtual void clone_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access) override;

    virtual Page_Table_Argumments craft_arguments() const override;

    /**
     * Returns the end byte of the memory object that is referenced by the region
     */
    inline u64 object_up_to() const noexcept
    {
        return start_offset_bytes + size;
    }

    virtual void prepare_deletion() noexcept override;

    constexpr bool can_takeout_page() const noexcept override
    {
        return false;
    }
};
