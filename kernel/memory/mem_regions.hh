#pragma once
#include <types.hh>
#include <lib/memory.hh>
#include <lib/string.hh>

class Port;
class TaskDescriptor;

extern u64 counter;

// Generic memory region class. Defines an action that needs to be executed when pagefault is produced.
struct Generic_Mem_Region {
    // Page aligned start of the region
    u64 start_addr = 0;

    // Page aligned size
    u64 size = 0;

    // Optional name of the region
    klib::string name;

    // Unique ID which identifies the region
    u64 id = 0;

    // Owner of the region
    u64 owner_cr3 = 0;

    // Flags for the region
    u8 access_type = 0;

    static constexpr u8 Readable   = 0x01;
    static constexpr u8 Writeable  = 0x02;
    static constexpr u8 Executable = 0x04;

    virtual kresult_t alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task) = 0;

    kresult_t prepare_page(u64 access_mode, u64 page_addr, const klib::shared_ptr<TaskDescriptor>& task);

    // Do action on page fault. If result != SUCCESS, kill the task.
    // Might do task switching
    kresult_t on_page_fault(u64 error, u64 pagefault_addr, const klib::shared_ptr<TaskDescriptor>& task);


    virtual ~Generic_Mem_Region();

    Generic_Mem_Region()
    {
        id = __atomic_add_fetch(&counter, 1, 0);
    }

    Generic_Mem_Region(u64 start_addr, u64 size, klib::string name, u64 owner_cr3, u8 access):
        start_addr(start_addr), size(size), name(klib::forward<klib::string>(name)), id(__atomic_add_fetch(&counter, 1, 0)), owner_cr3(owner_cr3), access_type(access)
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
};


// Physical memory mapping class.
struct Phys_Mapped_Region: Generic_Mem_Region {
    // Allocated a new page, pointing to the corresponding physical address.
    virtual kresult_t alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task);

    u64 phys_addr_start = 0;

    // Constructs a region with virtual address starting at *aligned_virt* of size *size* pointing to *aligned_phys*
    Phys_Mapped_Region(u64 aligned_virt, u64 size, u64 aligned_phys);

    Phys_Mapped_Region(u64 start_addr, u64 size, klib::string name, u64 owner_cr3, u8 access, u64 phys_addr_start):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner_cr3, access), phys_addr_start(phys_addr_start)
        {};
};


// Normal region. On page fault, attempts to allocate the page and fills it with specified pattern (e.g. zeroing) if successfull
struct Private_Normal_Region: Generic_Mem_Region {
    // This pattern is used to fill the newly allocated pages
    u64 pattern = 0;

    // Attempt to allocate a new page
    virtual kresult_t alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task);

    // Tries to preallocate all the pages
    void prefill();

    Private_Normal_Region(u64 start_addr, u64 size, klib::string name, u64 owner_cr3, u8 access, u64 pattern):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner_cr3, access), pattern(pattern)
        {};
};


// Managed private region. Pagefaults block the process and send the message to the managing process
struct Private_Managed_Region: Generic_Mem_Region {
    // When pagefaults occur, the notification is sent to this port
    klib::weak_ptr<Port> notifications_port;

    // When pagefault occurs to the current task, it is blocked and the message is sent to the *notifications_port*. 
    virtual kresult_t alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task);

    Private_Managed_Region(u64 start_addr, u64 size, klib::string name, u64 owner_cr3, u8 access, klib::shared_ptr<Port> p):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner_cr3, access), notifications_port(klib::forward<klib::shared_ptr<Port>>(p))
        {};
};