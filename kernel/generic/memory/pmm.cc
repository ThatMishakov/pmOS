#include "pmm.hh"

#include "bsearch.hh"

#include <sched/sched.hh>
#include <memory/paging.hh>
#include "mem_object.hh"

using namespace kernel;
using namespace kernel::pmm;

RegionsTree::RBTreeHead kernel::pmm::memory_regions;
PageLL kernel::pmm::free_pages_list[page_lists];

PageArrayDescriptor *kernel::pmm::phys_memory_regions = nullptr;
size_t kernel::pmm::phys_memory_regions_count         = 0;
size_t kernel::pmm::phys_memory_regions_capacity      = 0;

Page_Descriptor &Page_Descriptor::operator=(Page_Descriptor &&p) noexcept
{
    if (this == &p)
        return *this;

    try_free_page();

    page_struct_ptr   = p.page_struct_ptr;
    p.page_struct_ptr = nullptr;

    return *this;
}

ReturnStr<Page_Descriptor> Page_Descriptor::create_copy() const noexcept
{
    assert(page_struct_ptr);

    auto new_page = allocate_page(12);
    if (!new_page.success())
        return new_page.propagate();

    Temp_Mapper_Obj<char> this_mapping(request_temp_mapper());
    Temp_Mapper_Obj<char> new_mapping(request_temp_mapper());

    this_mapping.map(page_struct_ptr->get_phys_addr());
    new_mapping.map(new_page.val.page_struct_ptr->get_phys_addr());

    const auto page_size_bytes = 4096;

    memcpy(new_mapping.ptr, this_mapping.ptr, page_size_bytes);

    return new_page;
}

Page *Page_Info::get_page() const
{
    assert(is_allocated);
    return find_page(page_addr);
}

Page::page_addr_t Page_Descriptor::takeout_page() noexcept
{
    assert(page_struct_ptr);
    assert(page_struct_ptr->has_physical_page());
    assert(page_struct_ptr->type == Page::PageType::Allocated);

    auto p          = page_struct_ptr->get_phys_addr();
    page_struct_ptr = nullptr;
    return p;
}

ReturnStr<Page_Descriptor> Page_Descriptor::allocate_page(u8 alignment_log)
{
    assert(alignment_log == 12 && "Only 4K pages are currently supported");

    Page *p = pmm::alloc_pages(1);
    if (p == nullptr)
        return Error(-ENOMEM);

    p->type       = Page::PageType::Allocated;
    p->l          = {};
    p->l.owner    = nullptr;
    p->l.refcount = 1;

    return Page_Descriptor(p);
}

ReturnStr<Page_Descriptor> Page_Descriptor::allocate_page_zeroed(u8 alignment_log)
{
    assert(alignment_log == 12 && "Only 4K pages are currently supported");

    Page *p = pmm::alloc_pages(1);
    if (p == nullptr)
        return Error(-ENOMEM);

    p->type       = Page::PageType::Allocated;
    p->l          = {};
    p->l.owner    = nullptr;
    p->l.refcount = 1;

    Temp_Mapper_Obj<char> mapping(request_temp_mapper());
    mapping.map(p->get_phys_addr());
    memset(mapping.ptr, 0, PAGE_SIZE);

    return Page_Descriptor(p);
}

Page_Descriptor::~Page_Descriptor() noexcept { try_free_page(); }

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

// TODO: This name is somewhat bad, since the page isn't actually allocated...
Page_Descriptor Page_Descriptor::create_from_allocated(Page::page_addr_t phys_addr)
{
    auto s = find_page_struct(phys_addr);
    assert(s.page_struct_ptr);
    __atomic_sub_fetch(&s.page_struct_ptr->l.refcount, 1, __ATOMIC_SEQ_CST);
    return s;
}

Page_Descriptor Page_Descriptor::create_empty() noexcept
{
    klib::unique_ptr<Page> new_page_struct = klib::make_unique<Page>();
    if (!new_page_struct)
        return Page_Descriptor::none();

    new_page_struct->type       = Page::PageType::Allocated;
    new_page_struct->flags      = Page::FLAG_NO_PAGE;
    new_page_struct->l.owner    = nullptr;
    new_page_struct->l.refcount = 1;

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

void pmm::release_page(Page *page) noexcept
{
    if (!page)
        return;

    if (not page->has_physical_page()) {
        delete page;
    } else {
        assert(page->type == Page::PageType::Allocated);
        if (page->is_anonymous() and page->l.owner != nullptr)
            page->l.owner->atomic_remove_anonymous_page(page);

        page->type                    = Page::PageType::PendingFree;
        RCU_Head &rcu                 = page->rcu_state.rcu_h;
        rcu.rcu_func                  = Page::rcu_callback;
        rcu.rcu_next                  = nullptr;
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
    auto new_page = Page_Descriptor::allocate_page(12);
    if (!new_page.success())
        return {};

    Temp_Mapper_Obj<char> this_mapping(request_temp_mapper());
    Temp_Mapper_Obj<char> new_mapping(request_temp_mapper());

    this_mapping.map(page_addr);
    new_mapping.map(new_page.val.page_struct_ptr->get_phys_addr());

    const auto page_size_bytes = 4096;

    memcpy(new_mapping.ptr, this_mapping.ptr, page_size_bytes);

    return klib::move(new_page.val);
}

bool phys_memory_regions_empty() noexcept { return phys_memory_regions_count == 0; }
PageArrayDescriptor *phys_memory_regions_begin() noexcept { return phys_memory_regions; }
PageArrayDescriptor *phys_memory_regions_end() noexcept
{
    return phys_memory_regions + phys_memory_regions_count;
}

bool kernel::pmm::add_page_array(Page::page_addr_t start_addr, u64 size, Page *pages) noexcept
{
    if (phys_memory_regions_count == phys_memory_regions_capacity) {
        auto old_capacity = phys_memory_regions_capacity;
        auto new_capacity = phys_memory_regions_capacity + 8;
        auto new_regions =
            (PageArrayDescriptor *)malloc(new_capacity * sizeof(PageArrayDescriptor));
        if (!new_regions)
            return false;

        memcpy(new_regions, phys_memory_regions,
               phys_memory_regions_count * sizeof(PageArrayDescriptor));
        free(phys_memory_regions);
        phys_memory_regions          = new_regions;
        phys_memory_regions_capacity = new_capacity;

        memory_regions = {};
        for (size_t i = 0; i < old_capacity; i++) {
            memory_regions.insert(&phys_memory_regions[i]);
        }
    }

    auto &r      = phys_memory_regions[phys_memory_regions_count++];
    r.pages      = pages;
    r.start_addr = start_addr;
    r.size       = size;

    memory_regions.insert(&r);
    return true;
}

template<typename RandomIterator, class T, class Compare>
static RandomIterator lower_or_equal(RandomIterator begin, RandomIterator end, T val,
                                     Compare cmp) noexcept
{
    RandomIterator left  = begin;
    RandomIterator right = end;

    RandomIterator result = begin;

    while (left < right) {
        RandomIterator mid = left + (right - left) / 2;
        if (cmp(val, *mid)) {
            right = mid;
        } else {
            result = mid;
            left   = mid + 1;
        }
    }

    return result;
}

Page::page_addr_t Page::get_phys_addr() const noexcept
{
    assert(has_physical_page());

    auto it = memory_regions.get_smaller_or_equal(this);
    assert(it);

    assert(this >= it->pages and this < it->pages + it->size);

    auto diff = this - it->pages;
    assert(diff < (long int)it->size);
    return diff * PAGE_SIZE + it->start_addr;
}

Spinlock pmm_lock;

void kernel::pmm::free_page(Page *p) noexcept
{
    if (!p)
        return;

    Auto_Lock_Scope l(pmm_lock);

    Page *next          = nullptr;
    size_t num_of_pages = 0;

    if (p->type == Page::PageType::AllocatedPending) {
        next         = p->pending_alloc_head.next;
        num_of_pages = p->pending_alloc_head.size_pages;
    } else if (p->type == Page::PageType::PendingFree) {
        num_of_pages = p->rcu_state.pages_to_free;
    } else {
        assert(!"Invalid page type");
    }

    // Create free entry
    p->type                  = Page::PageType::Free;
    p->free_region_head      = {};
    p->free_region_head.size = num_of_pages;
    p->flags                 = 0;

    // Coalesce region before the pages
    size_t coalesce_old_size = -1UL;
    auto region_before       = p - 1;
    if (region_before->type == Page::PageType::Free) {
        p -= region_before->free_region_head.size;

        assert(p->type == Page::PageType::Free);
        assert(p->free_region_head.size == region_before->free_region_head.size);

        coalesce_old_size = region_before->free_region_head.size;
        p->free_region_head.size += num_of_pages;
    }

    // Coalesce region after the pages
    auto region_after = p + p->free_region_head.size;
    if (region_after->type == Page::PageType::Free) {
        free_pages_list->remove(region_after);

        auto region_after_after = region_after + region_after->free_region_head.size - 1;
        assert(region_after_after->type == Page::PageType::Free);
        assert(region_after_after->free_region_head.size == region_after->free_region_head.size);

        p->free_region_head.size += region_after->free_region_head.size;
    }

    // I am certainly not very good at naming variables
    if (coalesce_old_size == -1UL) {
        auto order = log2(p->free_region_head.size);
        if (order >= page_lists)
            order = page_lists - 1;
        free_pages_list[order].push_front(p);
    } else {
        auto old_order = log2(coalesce_old_size);
        if (old_order >= page_lists)
            old_order = page_lists - 1;

        int new_order = log2(p->free_region_head.size);
        if (new_order >= page_lists)
            new_order = page_lists - 1;

        if (old_order != (page_lists - 1) and old_order < new_order) {

            free_pages_list[old_order].remove(p);
            free_pages_list[new_order].push_front(p);
        }
    }

    auto end                   = p + p->free_region_head.size - 1;
    end->type                  = Page::PageType::Free;
    end->free_region_head.size = p->free_region_head.size;

    // TODO: This will deadlock
    free_page(next);
}

void Page::rcu_callback(void *ptr, bool) noexcept
{
    auto page = reinterpret_cast<Page *>((char *)ptr - offsetof(Page, rcu_state.rcu_h));
    // TODO: Free pages in chains

    free_page(page);
}

Page *kernel::pmm::find_page(Page::page_addr_t addr) noexcept
{
    if (phys_memory_regions_empty()) [[unlikely]]
        return nullptr;

    auto it = lower_or_equal(phys_memory_regions_begin(), phys_memory_regions_end(), addr,
                             [](const auto &a, const auto &b) { return a < b.start_addr; });
    if (it == phys_memory_regions_end())
        --it;

    if (it->start_addr <= addr && addr < (it->start_addr + it->size * PAGE_SIZE)) [[likely]] {
        auto diff = addr - it->start_addr;
        return it->pages + diff / PAGE_SIZE;
    }

    return nullptr;
}

#include <kern_logger/kern_logger.hh>
Page_Descriptor Page_Descriptor::find_page_struct(Page::page_addr_t addr) noexcept
{
    auto p = find_page(addr);
    if (!p) [[unlikely]]
        return Page_Descriptor();

    if (p->type != Page::PageType::Allocated) [[unlikely]] {
        serial_logger.printf("!!! Page %p at addr %h is not allocated !!!. Type: %i\n", p, addr,
                             p->type);
        for (size_t i = 0; i < sizeof(Page); i++)
            serial_logger.printf("%h ", ((u8 *)p)[i]);
        serial_logger.printf("\n");
    }

    assert(p->type == Page::PageType::Allocated);

    __atomic_add_fetch(&p->l.refcount, 1, __ATOMIC_SEQ_CST);
    return Page_Descriptor(p);
}

void kernel::pmm::free_memory_for_kernel(phys_page_t page, size_t number_of_pages) noexcept
{
    // TODO: This function can crash kernel if allocation fails early in the boot
    auto p = find_page(page);
    assert(p);

    p->type          = Page::PageType::PendingFree;
    auto &&r         = p->rcu_state;
    r.pages_to_free  = number_of_pages;
    r.rcu_h.rcu_func = Page::rcu_callback;
    r.rcu_h.rcu_next = nullptr;

    get_cpu_struct()->paging_rcu_cpu.push(&r.rcu_h);
}

bool kernel::pmm::pmm_fully_initialized = false;
Page::page_addr_t alloc_pages_from_temp_pool(size_t pages) noexcept;

Page::page_addr_t kernel::pmm::get_memory_for_kernel(size_t pages) noexcept
{
    assert(pages > 0);

    if (!pmm_fully_initialized) [[unlikely]] {
        return alloc_pages_from_temp_pool(pages);
    }

    Page *p = alloc_pages(pages, true);
    if (!p) [[unlikely]]
        return -1UL;

    for (size_t i = 0; i < pages; i++) {
        p[i].type       = Page::PageType::Allocated;
        p[i].flags      = 0;
        p[i].l.owner    = nullptr;
        p[i].l.refcount = 1;
    }

    return phys_of_page(p);
}

Page::page_addr_t kernel::pmm::phys_of_page(Page *p) noexcept { return p->get_phys_addr(); }

Page *kernel::pmm::alloc_pages(size_t count, bool /* contiguous */) noexcept
{
    assert(count > 0);

    Auto_Lock_Scope l(pmm_lock);

    if (phys_memory_regions_empty()) [[unlikely]]
        return nullptr;

    // TODO: Only contiguous pages are supported for now
    // Non-contiguous can probably be implemented quite easily, but it only matters
    // for large allocations, when the system is low on memory
    // (may be not)

    if (count > (1 << page_lists)) [[unlikely]]
        // Allocation above 1GB in size
        // Again, this should not be hard to implement, but doesn't matter for now
        return nullptr;

    auto order = log2(count);
    if ((1UL << order) < count)
        order++;

    // The first entry in the list will always satisfy the allocation
    // TODO: Add a bitmap to speed up the search
    for (size_t i = order; i < page_lists; i++) {
        if (!free_pages_list[i].empty()) {
            auto p = &free_pages_list[i].front();
            if (p->free_region_head.size > count) {
                p->free_region_head.size -= count;
                auto t = p + p->free_region_head.size - 1;

                t->free_region_head.size = p->free_region_head.size;
                t->type                  = Page::PageType::Free;

                if (p->free_region_head.size < (1UL << i)) {
                    free_pages_list[i].remove(p);
                    auto new_order = log2(p->free_region_head.size);
                    assert((size_t)new_order < i);
                    free_pages_list[new_order].push_front(p);
                }

                p += p->free_region_head.size;
            } else {
                free_pages_list[i].remove(p);
            }

            p->type                          = Page::PageType::AllocatedPending;
            p->flags                         = 0;
            p->pending_alloc_head.size_pages = count;
            p->pending_alloc_head.phys_addr  = p->get_phys_addr();
            p->pending_alloc_head.next       = nullptr;

            // The first and the last pages of the regions are free (for faster merging), so mark
            // them as not
            p[count - 1].type = Page::PageType::AllocatedPending;
            return p;
        }
    }

    return nullptr;
}

Page_Descriptor::operator bool() const noexcept { return page_struct_ptr != nullptr; }

Page::page_addr_t Page_Descriptor::get_phys_addr() const noexcept
{
    assert(page_struct_ptr);
    return page_struct_ptr->get_phys_addr();
}