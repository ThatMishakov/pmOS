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

#include "page_descriptor.hh"

#include "mem.hh"
#include "temp_mapper.hh"

#include <assert.h>
#include <sched/sched.hh>
#include <utils.hh>

Page_Descriptor::~Page_Descriptor() noexcept { try_free_page(); }

void Page_Descriptor::try_free_page() noexcept
{
    if (owning) {
        assert(alignment_log == 12 && "Only 4K pages are currently supported");

        kernel_pframe_allocator.free((void *)page_ptr);
    }
}

Page_Descriptor &Page_Descriptor::operator=(Page_Descriptor &&p) noexcept
{
    if (this == &p)
        return *this;

    try_free_page();

    available     = p.available;
    owning        = p.owning;
    alignment_log = p.alignment_log;
    page_ptr      = p.page_ptr;

    p.available     = false;
    p.owning        = false;
    p.alignment_log = 0;
    p.page_ptr      = 0;

    return *this;
}

Page_Descriptor Page_Descriptor::create_copy() const
{
    assert(available && "page must be available");

    assert(alignment_log && "only 4K pages are supported");

    Page_Descriptor new_page(true,                                     // available
                             true,                                     // owning
                             alignment_log,                            // alignment_log
                             (u64)kernel_pframe_allocator.alloc_page() // page_ptr
    );

    Temp_Mapper_Obj<char> this_mapping(request_temp_mapper());
    Temp_Mapper_Obj<char> new_mapping(request_temp_mapper());

    this_mapping.map(page_ptr);
    new_mapping.map(new_page.page_ptr);

    const auto page_size_bytes = 4096;

    memcpy(new_mapping.ptr, this_mapping.ptr, page_size_bytes);

    return new_page;
}

klib::pair<u64 /* page_ppn */, bool /* owning reference */> Page_Descriptor::takeout_page() noexcept
{
    const klib::pair<u64, bool> p = {page_ptr, available};

    owning = false;
    *this  = Page_Descriptor();

    return p;
}

Page_Descriptor Page_Descriptor::allocate_page(u8 alignment_log)
{
    assert(alignment_log == 12 && "Only 4K pages are currently supported");

    void *page = kernel_pframe_allocator.alloc_page();

    return Page_Descriptor(true, true, alignment_log, (u64)page);
}