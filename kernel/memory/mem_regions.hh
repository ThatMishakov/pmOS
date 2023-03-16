#pragma once
#include <types.hh>
#include <lib/memory.hh>
#include <lib/string.hh>

class Port;
class TaskDescriptor;
class Page_Table;

extern u64 counter;
struct Page_Table_Argumments;

// Generic memory region class. Defines an action that needs to be executed when pagefault is produced.
struct Generic_Mem_Region: public klib::enable_shared_from_this<Generic_Mem_Region> {
    // Page aligned start of the region
    u64 start_addr = 0;

    // Page aligned size
    u64 size = 0;

    // Optional name of the region
    klib::string name;

    // Unique ID which identifies the region
    u64 id = 0;

    // Owner of the region
    klib::weak_ptr<Page_Table> owner;

    // Flags for the region
    u8 access_type = 0;

    static constexpr u8 Readable   = 0x01;
    static constexpr u8 Writeable  = 0x02;
    static constexpr u8 Executable = 0x04;

    virtual bool alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task) = 0;
    constexpr virtual bool can_takeout_page() const
    {
        return false;
    }

    bool prepare_page(u64 access_mode, u64 page_addr, const klib::shared_ptr<TaskDescriptor>& task);

    // Do action on page fault. If result != SUCCESS, kill the task.
    // Might do task switching
    bool on_page_fault(u64 error, u64 pagefault_addr, const klib::shared_ptr<TaskDescriptor>& task);


    virtual ~Generic_Mem_Region() = default;

    Generic_Mem_Region()
    {
        id = __atomic_add_fetch(&counter, 1, 0);
    }

    Generic_Mem_Region(u64 start_addr, u64 size, klib::string name, klib::weak_ptr<Page_Table> owner, u8 access):
        start_addr(start_addr), size(size), name(klib::forward<klib::string>(name)), id(__atomic_add_fetch(&counter, 1, 0)), owner(klib::forward<klib::weak_ptr<Page_Table>>(owner)), access_type(access)
        {};

    bool is_in_range(u64 addr) const
    {
        return addr >= start_addr and addr < start_addr+size;
    }

    bool has_permissions(u64 err_code) const
    {
        return (not ((err_code & 0x02) and not (access_type & Writeable))) and (not ((err_code & 0x10) and not (access_type & Executable)));
    }

    bool has_access(unsigned access_mask) const
    {
        return not (access_mask & ~this->access_type);
    }

    static inline bool protection_violation(u64 err_code)
    {
        return err_code & 0x01;
    }

    u64 addr_end() const
    {
        return start_addr + size;
    }

    virtual Page_Table_Argumments craft_arguments() const = 0;

    constexpr virtual bool is_managed() const
    {
        return false;
    }

    virtual void move_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access);
};


// Physical memory mapping class.
struct Phys_Mapped_Region: Generic_Mem_Region {
    // Allocated a new page, pointing to the corresponding physical address.
    virtual bool alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task);

    u64 phys_addr_start = 0;
    constexpr bool can_takeout_page() const override
    {
        return false;
    }

    virtual Page_Table_Argumments craft_arguments() const;

    // Constructs a region with virtual address starting at *aligned_virt* of size *size* pointing to *aligned_phys*
    Phys_Mapped_Region(u64 aligned_virt, u64 size, u64 aligned_phys);

    Phys_Mapped_Region(u64 start_addr, u64 size, klib::string name, klib::weak_ptr<Page_Table> owner, u8 access, u64 phys_addr_start):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), klib::forward<klib::weak_ptr<Page_Table>>(owner), access), phys_addr_start(phys_addr_start)
        {};
};


// Normal region. On page fault, attempts to allocate the page and fills it with specified pattern (e.g. zeroing) if successfull
struct Private_Normal_Region: Generic_Mem_Region {
    // This pattern is used to fill the newly allocated pages
    u64 pattern = 0;

    constexpr bool can_takeout_page() const override
    {
        return true;
    }

    // Attempt to allocate a new page
    virtual bool alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task);

    // Tries to preallocate all the pages
    void prefill();

    virtual Page_Table_Argumments craft_arguments() const;

    Private_Normal_Region(u64 start_addr, u64 size, klib::string name, klib::weak_ptr<Page_Table> owner, u8 access, u64 pattern):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), klib::forward<klib::weak_ptr<Page_Table>>(owner), access), pattern(pattern)
        {};
};


// Managed private region. Pagefaults block the process and send the message to the managing process
struct Private_Managed_Region: Generic_Mem_Region {
    // When pagefaults occur, the notification is sent to this port
    klib::weak_ptr<Port> notifications_port;

    constexpr bool can_takeout_page() const override
    {
        return false;
    }

    constexpr bool is_managed() const override
    {
        return true;
    }

    virtual Page_Table_Argumments craft_arguments() const;

    // When pagefault occurs to the current task, it is blocked and the message is sent to the *notifications_port*. 
    virtual bool alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task);

    Private_Managed_Region(u64 start_addr, u64 size, klib::string name, klib::weak_ptr<Page_Table> owner, u8 access, klib::shared_ptr<Port> p):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), klib::forward<klib::weak_ptr<Page_Table>>(owner), access), notifications_port(klib::forward<klib::shared_ptr<Port>>(p))
        {};

    virtual void move_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access) override;
};