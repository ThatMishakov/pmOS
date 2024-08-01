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

#include "pmm.hh"
#include "temp_mapper.hh"

#include <assert.h>
#include <sched/sched.hh>
#include <utils.hh>

using namespace kernel;

Page_Descriptor &Page_Descriptor::operator=(Page_Descriptor &&p) noexcept
{
    if (this == &p)
        return *this;

    try_free_page();

    page_struct_ptr   = p.page_struct_ptr;
    p.page_struct_ptr = nullptr;

    return *this;
}

Page_Descriptor Page_Descriptor::create_copy() const
{
    assert(page_struct_ptr);

    auto new_page = allocate_page(12);

    Temp_Mapper_Obj<char> this_mapping(request_temp_mapper());
    Temp_Mapper_Obj<char> new_mapping(request_temp_mapper());

    this_mapping.map(page_struct_ptr->get_phys_addr());
    new_mapping.map(new_page.page_struct_ptr->get_phys_addr());

    const auto page_size_bytes = 4096;

    memcpy(new_mapping.ptr, this_mapping.ptr, page_size_bytes);

    return new_page;
}

Page::page_addr_t Page_Descriptor::takeout_page() noexcept
{
    assert(page_struct_ptr);
    assert(page_struct_ptr->has_physical_page());

    auto p          = page_struct_ptr->get_phys_addr();
    page_struct_ptr = nullptr;
    return p;
}

Page_Descriptor Page_Descriptor::allocate_page(u8 alignment_log)
{
    assert(alignment_log == 12 && "Only 4K pages are currently supported");

    Page *p = pmm::alloc_pages(1);
    if (p == nullptr)
        throw Kern_Exception(-ENOMEM, "Failed to allocate page");

    p->type = Page::PageType::Allocated;
    p->l = {};
    p->l.refcount = 1;

    return Page_Descriptor(p);
}

Page_Descriptor Page_Descriptor::allocate_page_zeroed(u8 alignment_log)
{
    assert(alignment_log == 12 && "Only 4K pages are currently supported");

    Page *p = pmm::alloc_pages(1);
    if (p == nullptr)
        throw Kern_Exception(-ENOMEM, "Failed to allocate page");

    p->type = Page::PageType::Allocated;
    p->l = {};
    p->l.refcount = 1;

    Temp_Mapper_Obj<char> mapping(request_temp_mapper());
    mapping.map(p->get_phys_addr());
    memset(mapping.ptr, 0, PAGE_SIZE);

    return Page_Descriptor(p);
}


Page_Descriptor::~Page_Descriptor()
{
    try_free_page();
}

Page_Descriptor::Page_Descriptor(Page_Descriptor &&p) noexcept: page_struct_ptr(p.page_struct_ptr)
{
    p.page_struct_ptr = nullptr;
}

void Page_Descriptor::try_free_page() noexcept
{
    if (page_struct_ptr == nullptr)
        return;

    assert(page_struct_ptr->type == Page::PageType::Allocated);

    auto i = __atomic_sub_fetch(&page_struct_ptr->l.refcount, 1, __ATOMIC_SEQ_CST);
    if (i == 0)
        release_page(page_struct_ptr);

    page_struct_ptr = nullptr;
}

// Page_Descriptor Page_Descriptor::create_from_allocated(Page::page_addr_t phys_addr)
// {
//     klib::unique_ptr<Page> new_page_struct = klib::make_unique<Page>();
//     new_page_struct->page_ptr              = phys_addr;
//     new_page_struct->refcount              = 1;

//     insert_global_pages_list(new_page_struct.get());

//     return Page_Descriptor(new_page_struct.release());
// }

Page_Descriptor Page_Descriptor::create_empty() noexcept
{
    klib::unique_ptr<Page> new_page_struct = klib::make_unique<Page>();
    if (!new_page_struct)
        return Page_Descriptor::none();
    
    new_page_struct->type                  = Page::PageType::Allocated;
    new_page_struct->flags = Page::FLAG_NO_PAGE;
    new_page_struct->l.refcount              = 1;

    return Page_Descriptor(new_page_struct.release());
}

Page_Descriptor Page_Descriptor::duplicate() const noexcept
{
    if (page_struct_ptr == nullptr)
        return Page_Descriptor::none();

    __atomic_add_fetch(&page_struct_ptr->l.refcount, 1, __ATOMIC_SEQ_CST);
    return Page_Descriptor(page_struct_ptr);
}

void Page_Descriptor::release_taken_out_page()
{
    if (page_struct_ptr == nullptr)
        return;

    assert(page_struct_ptr->type == Page::PageType::Allocated);
    assert(page_struct_ptr->l.refcount > 1);

    __atomic_sub_fetch(&page_struct_ptr->l.refcount, 1, __ATOMIC_SEQ_CST);
}

void release_page(Page *page) noexcept
{
    if (not page->has_physical_page()) {
        delete page;
    } else {
        RCU_Head &rcu = page->rcu_state.rcu_h;
        rcu.rcu_func = Page::rcu_callback;
        rcu.rcu_next = nullptr;
        page->rcu_state.pages_to_free = 1;

        get_cpu_struct()->paging_rcu_cpu.push(&rcu);
    }
}

Page_Descriptor Page_Descriptor::dup_from_raw_ptr(Page *p) noexcept
{
    if (p == nullptr)
        return Page_Descriptor::none();

    __atomic_add_fetch(&p->l.refcount, 1, __ATOMIC_SEQ_CST);
    return Page_Descriptor(p);
}

Page_Descriptor Page_Table::Page_Info::create_copy() const
{
    if (not is_allocated)
        return {};

    assert(!nofree);

    // Return a copy
    Page_Descriptor new_page = Page_Descriptor::allocate_page(12);

    Temp_Mapper_Obj<char> this_mapping(request_temp_mapper());
    Temp_Mapper_Obj<char> new_mapping(request_temp_mapper());

    this_mapping.map(page_addr);
    new_mapping.map(new_page.page_struct_ptr->get_phys_addr());

    const auto page_size_bytes = 4096;

    memcpy(new_mapping.ptr, this_mapping.ptr, page_size_bytes);

    return new_page;
}