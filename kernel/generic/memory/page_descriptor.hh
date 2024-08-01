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
#include "rcu.hh"

#include <cstddef>
#include <types.hh>
#include "intrusive_list.hh"

using phys_page_t = u64;

struct Page {
    static void rcu_callback(void *self, bool chained) noexcept;
    Page() noexcept = default;

    using page_addr_t = u64;

    enum class PageType {
        Free = 0,
        PendingFree,
        Allocated,
        Reserved, // First and last pages of regions
    };

    struct Mem_Object_LL_Head {
        Page *next;
        u64 offset;
        size_t refcount;
    };

    struct PendingAllocHead {
        Page *next;
        page_addr_t phys_addr;
    };

    struct FreeRegionHead {
        DoubleListHead<Page> list;
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
        FreeRegionHead free_region_head;
    };

    PageType type = PageType::Free;
    int flags     = 0;

    static constexpr int FLAG_NO_PAGE = 1 << 0;

    bool has_physical_page() const noexcept { return !(flags & FLAG_NO_PAGE); }

    page_addr_t get_phys_addr() const noexcept;
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

    /// Allocates a new page and copies current page into it
    Page_Descriptor create_copy() const;

    /// Creates another reference to the page
    Page_Descriptor duplicate() const noexcept;

    /// Frees the page that is held by the descriptor if it is managed
    void try_free_page() noexcept;

    /// @brief Allocates a page
    /// @param alignment_log Logarithm of the alignment of the page (e.g. 12 for 4096 bytes)
    /// @return Newly allocated page. Throws on error
    static Page_Descriptor allocate_page(u8 alignment_log);

    static Page_Descriptor allocate_page_zeroed(u8 alignment_log);

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
    Page::page_addr_t get_phys_addr() noexcept;

    /// Creates a page descriptor from the pointer to the page, taking ownerhip of it
    static Page_Descriptor from_raw_ptr(Page *p) noexcept { return Page_Descriptor(p); }

    static Page_Descriptor dup_from_raw_ptr(Page *p) noexcept;

    static Page_Descriptor none() { return Page_Descriptor(nullptr); }
};