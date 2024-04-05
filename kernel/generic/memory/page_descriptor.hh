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

    /// @brief Allocates a page
    /// @param alignment_log Logarithm of the alignment of the page (e.g. 12 for 4096 bytes)
    /// @return Newly allocated page. Throws on error
    static Page_Descriptor allocate_page(u8 alignment_log);
};