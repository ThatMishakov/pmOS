#include "page_descriptor.hh"
#include "mem.hh"
#include <assert.h>
#include "temp_mapper.hh"
#include <utils.hh>
#include <sched/sched.hh>

Page_Descriptor::~Page_Descriptor() noexcept
{
    try_free_page();
}

void Page_Descriptor::try_free_page() noexcept
{
    if (owning) {
        assert(alignment_log == 12 && "Only 4K pages are currently supported");

        kernel_pframe_allocator.free((void*)page_ptr);
    }    
}

Page_Descriptor& Page_Descriptor::operator=(Page_Descriptor &&p) noexcept
{
    if (this == &p)
        return *this;

    try_free_page();

    available = p.available;
    owning = p.owning;
    alignment_log = p.alignment_log;
    page_ptr = p.page_ptr;

    p.available = false;
    p.owning = false;
    p.alignment_log = 0;
    p.page_ptr = 0;

    return *this;
}

Page_Descriptor Page_Descriptor::create_copy() const
{
    assert(available && "page must be available");
    
    assert(alignment_log && "only 4K pages are supported");

    Page_Descriptor new_page (
        true, // available
        true, // owning
        alignment_log, // alignment_log
        (u64)kernel_pframe_allocator.alloc_page() // page_ptr
    );

    Temp_Mapper_Obj<char> this_mapping(get_cpu_struct()->temp_mapper);
    Temp_Mapper_Obj<char> new_mapping(get_cpu_struct()->temp_mapper);

    this_mapping.map(page_ptr);
    new_mapping.map(new_page.page_ptr);

    const auto page_size_bytes = 4096;

    memcpy(new_mapping.ptr, this_mapping.ptr, page_size_bytes);

    return new_page;
}

klib::pair<u64 /* page_ppn */, bool /* owning reference */> Page_Descriptor::takeout_page() noexcept
{
    const klib::pair<u64, bool> p = {page_ptr, available};

    *this = Page_Descriptor();

    return p;
}