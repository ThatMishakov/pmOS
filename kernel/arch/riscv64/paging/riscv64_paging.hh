#pragma once
#include <types.hh>
#include <memory/paging.hh>

struct RISCV64_PTE {
    bool valid : 1 = false; // Indicates if the PTE (page table entry) is valid.
                            // If not set, the other bits are ignored and can be
                            // used for other purposes by the OS

    bool readable : 1   = false;  // RWE bits define permissions. Set to 0b000 they 
    bool writeable : 1   = false; // are an indication that PTE points
    bool executable : 1 = false;  // to a leaf PTE. Otherwise, if not a leaf
                                  // table has them, this is a huge page.

    bool user : 1 = false; // Specifies if the page is accessible by user mode
                           // Note: Risk-V requires the kernel to set the SUM
                            // bit in the sstatus register to allow accessing
                            // user mode pages while in kernel

    bool global : 1 = false; // If set, the page is global and survives
                             // TLB flushes. If the top level PTE has it set,
                             // the leaf entries are also treated as global.

    bool accessed : 1 = false; // Indicates that the page has been read, written
                               // or fetched since the bit was clear

    bool dirty : 1 = false; // Indicates that the page has been written since
                            // the bit was clear

    u8 available : 2 = 0; // Available for software use
                          // pmOS uses them to indicate the physical mapping pages
                          // and shared pages

    u64 ppn : 44 = 0; // Physical page number. Note: On x86, it starts at 12th bit,
                      // on RISC-V it's the 10th. Can be calculated by shifting the
                      // start of the physical address to the right by 12 bits

    u16 reserved : 7 = 0; // Reserved for future use. Must be 0
    u8 pbmt : 2 = 0; // Page-based memory type. 0 if unused
    bool napot : 1 = false; // Svnapot extension stuff. Must be zero if unused

    inline bool is_leaf() const noexcept
    {
        return readable or writeable or executable;
    }

    inline bool is_special() const noexcept
    {
        return available != 0;
    }
} __attribute__((packed, aligned(8)));

// Active number of paging levels of the system, e.g. 4 for SV48.
// At this moment, 4 is hardcoded, but this can be adjusted during the boot
// tu support SV57 or SV39 on systems with little memory
inline u8 riscv64_paging_levels = 4;

// Flushes the given page from the TLB cache
// This is a pinpoint operation, and only takes care of the hart it is ran on. If the page could be used by other CPUs,
// an IPI needs to be sent...
void flush_page(void * virt_addr) noexcept;

// Prepares a leaf PT for a given virtual address. This function is used during temp mapper initialization.
u64 prepare_leaf_pt_for(void * virt_addr, Page_Table_Argumments arg, u64 pt_ptr) noexcept;

// Maps a page to a given virtual address, using the available temporary maper.
// riscv64_paging_levels is used to determine the number of levels of the page table
// This function can allocate pages for leaf entries, if not already installed
kresult_t riscv_map_page(u64 pt_top_phys, void * virt_addr, u64 phys_addr, Page_Table_Argumments arg) noexcept;

// Unmaps the page from the given virtual address, using the available temporary maper.
// If the page is not special, it's freed
kresult_t riscv_unmap_page(u64 pt_top_phys, void * virt_addr) noexcept;

// Gets the top level page table pointer for the current hart
u64 get_current_hart_pt() noexcept;
