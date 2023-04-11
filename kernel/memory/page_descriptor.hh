#pragma once
#include <types.hh>
#include <lib/pair.hh>


/// Structure describing a page. If it owns a page, the destructor automatically dealocates it
struct Page_Descriptor {
    bool available = false;
    bool owning = false;
    u8 alignment_log = 0;
    u64 page_ptr = 0;

    Page_Descriptor() noexcept = default;
    Page_Descriptor(bool available, bool owning, u8 alignment_log, u64 page_ptr) noexcept:
        available(available), owning(owning), alignment_log(alignment_log), page_ptr(page_ptr) {}

    Page_Descriptor(const Page_Descriptor&) = delete;

    Page_Descriptor(Page_Descriptor&& r) noexcept:
        available(r.available), owning(r.owning), alignment_log(r.alignment_log), page_ptr(r.page_ptr)
        {r.available = false, r.owning = false, r.alignment_log = 0, r.page_ptr = 0;}

    Page_Descriptor& operator=(Page_Descriptor&&) noexcept;

    ~Page_Descriptor() noexcept;

    /// Takes the page out of the descriptor 
    klib::pair<u64 /* page_ppn */, bool /* owning reference */> takeout_page() noexcept;

    /// Allocates a new page and copies current page into it 
    Page_Descriptor create_copy() const;

    /// Frees the page that is held by the descriptor if it is managed
    void try_free_page() noexcept;
};