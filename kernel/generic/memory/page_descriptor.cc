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

    this_mapping.map(page_struct_ptr->page_ptr);
    new_mapping.map(new_page.page_struct_ptr->page_ptr);

    const auto page_size_bytes = 4096;

    memcpy(new_mapping.ptr, this_mapping.ptr, page_size_bytes);

    return new_page;
}

Page::page_addr_t Page_Descriptor::takeout_page() noexcept
{
    assert(page_struct_ptr);
    assert(page_struct_ptr->page_ptr != (u64)-1UL);

    auto p          = page_struct_ptr->page_ptr;
    page_struct_ptr = nullptr;
    return p;
}

Page_Descriptor Page_Descriptor::allocate_page(u8 alignment_log)
{
    assert(alignment_log == 12 && "Only 4K pages are currently supported");

    klib::unique_ptr<Page> new_page_struct = klib::make_unique<Page>();
    void *page                             = kernel_pframe_allocator.alloc_page();

    new_page_struct->page_ptr = (u64)page;
    new_page_struct->refcount = 1;

    insert_global_pages_list(new_page_struct.get());

    return Page_Descriptor(new_page_struct.release());
}

Page_Descriptor::~Page_Descriptor()
{
    if (page_struct_ptr == nullptr)
        return;

    auto i = __atomic_sub_fetch(&page_struct_ptr->refcount, 1, __ATOMIC_SEQ_CST);
    if (i == 0)
        release_page(page_struct_ptr);
}

Page_Descriptor::Page_Descriptor(Page_Descriptor &&p) noexcept: page_struct_ptr(p.page_struct_ptr)
{
    p.page_struct_ptr = nullptr;
}

void Page_Descriptor::try_free_page() noexcept
{
    if (page_struct_ptr == nullptr)
        return;

    auto i = __atomic_sub_fetch(&page_struct_ptr->refcount, 1, __ATOMIC_SEQ_CST);
    if (i == 0)
        release_page(page_struct_ptr);

    page_struct_ptr = nullptr;
}

Page_Descriptor Page_Descriptor::create_from_allocated(Page::page_addr_t phys_addr)
{
    klib::unique_ptr<Page> new_page_struct = klib::make_unique<Page>();
    new_page_struct->page_ptr              = phys_addr;
    new_page_struct->refcount              = 1;

    insert_global_pages_list(new_page_struct.get());

    return Page_Descriptor(new_page_struct.release());
}

Page_Descriptor Page_Descriptor::create_empty() noexcept
{
    klib::unique_ptr<Page> new_page_struct = klib::make_unique<Page>();
    new_page_struct->page_ptr              = -1UL;
    new_page_struct->refcount              = 1;

    return Page_Descriptor(new_page_struct.release());
}

Page_Descriptor Page_Descriptor::duplicate() const noexcept
{
    if (page_struct_ptr == nullptr)
        return Page_Descriptor::none();

    __atomic_add_fetch(&page_struct_ptr->refcount, 1, __ATOMIC_SEQ_CST);
    return Page_Descriptor(page_struct_ptr);
}

void Page_Descriptor::release_taken_out_page()
{
    if (page_struct_ptr == nullptr)
        return;

    assert(page_struct_ptr->refcount > 1);

    __atomic_sub_fetch(&page_struct_ptr->refcount, 1, __ATOMIC_SEQ_CST);
}

void release_page(Page *page) noexcept
{
    auto p = page->page_ptr;
    if (p == -1UL) {
        delete page;
    } else {
        remove_global_pages_list(page);
        RCU_Head &rcu = page->rcu_h;
        rcu.rcu_func = Page::rcu_callback;
        rcu.rcu_next = nullptr;

        get_cpu_struct()->paging_rcu_cpu.push(&rcu);
    }
}

void Page::rcu_callback(void *ptr) noexcept
{
    auto page = reinterpret_cast<Page *>((char *)ptr - offsetof(Page, rcu_h));
    kernel_pframe_allocator.free((void *)page->page_ptr);
    delete page;
}

klib::vector<Page *> global_pages_list {1024, nullptr};
size_t global_pages_list_count = 0;
Spinlock global_pages_list_lock;

size_t hash_index(Page *p)
{
    assert(p->page_ptr != (u64)-1UL);

    return ((p->page_ptr >> 12) * 7) % global_pages_list.size();
}

void insert_global_pages_list(Page *page) noexcept
{
    assert(page);
    Auto_Lock_Scope lock(global_pages_list_lock);

    // Vector is broken, so don't resize! :=P
    // try {
    //     if (global_pages_list_count * 4 >= global_pages_list.size() * 3) {
    //         serial_logger.printf("Resize! New size: %i\n", global_pages_list.size() * 2);
    //         global_pages_list.resize(global_pages_list.size() * 2);

    //         // Rehash
    //         Page *p = nullptr, *ll = nullptr;
    //         for (size_t i = 0; i < global_pages_list.size() / 2; i++) {
    //             while ((p = global_pages_list[i]) != nullptr) {
    //                 global_pages_list[i] = p->hashmap_next;
    //                 p->hashmap_next      = ll;
    //                 ll                   = p;
    //             }
    //         }

    //         while ((p = ll) != nullptr) {
    //             ll                     = p->hashmap_next;
    //             const size_t idx       = hash_index(p);
    //             p->hashmap_next        = global_pages_list[idx];
    //             global_pages_list[idx] = p;
    //         }
    //     }
    // } catch (...) {
    // }

    size_t idx             = hash_index(page);
    page->hashmap_next     = global_pages_list[idx];
    global_pages_list[idx] = page;
    global_pages_list_count++;
}

void remove_global_pages_list(Page *page) noexcept
{
    assert(page);
    Auto_Lock_Scope lock(global_pages_list_lock);

    size_t idx = hash_index(page);
    Page *p = global_pages_list[idx], *ll = nullptr;

    while (p != page) {
        if (p == nullptr)
            return;

        ll = p;
        p  = p->hashmap_next;
    }

    if (ll == nullptr)
        global_pages_list[idx] = p->hashmap_next;
    else
        ll->hashmap_next = p->hashmap_next;

    global_pages_list_count--;
}

static Page *find_page_in_global_list(Page::page_addr_t addr) noexcept
{
    Auto_Lock_Scope lock(global_pages_list_lock);

    const size_t idx = ((addr >> 12) * 7) % global_pages_list.size();
    Page *p          = global_pages_list[idx];

    while (p != nullptr) {
        if (p->page_ptr == addr)
            return p;

        p = p->hashmap_next;
    }

    return nullptr;
}

Page_Descriptor Page_Descriptor::find_page_struct(Page::page_addr_t addr) noexcept
{
    Page *p = find_page_in_global_list(addr);
    if (p == nullptr)
        return Page_Descriptor::none();

    __atomic_add_fetch(&p->refcount, 1, __ATOMIC_SEQ_CST);
    return Page_Descriptor(p);
}

Page_Descriptor Page_Descriptor::dup_from_raw_ptr(Page *p) noexcept
{
    if (p == nullptr)
        return Page_Descriptor::none();

    __atomic_add_fetch(&p->refcount, 1, __ATOMIC_SEQ_CST);
    return Page_Descriptor(p);
}