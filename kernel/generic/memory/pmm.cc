#include "pmm.hh"

#include "bsearch.hh"

#include <sched/sched.hh>

using namespace kernel;
using namespace kernel::pmm;

RegionsTree::RBTreeHead kernel::pmm::memory_regions;
klib::vector<PageArrayDescriptor> kernel::pmm::phys_memory_regions;
PageLL kernel::pmm::free_pages_list[page_lists];

Page::page_addr_t Page::get_phys_addr() const noexcept
{
    assert(has_physical_page());

    auto it = memory_regions.lower_bound(this);
    assert(it);

    auto diff = this - it->pages;
    assert(diff < (long int)it->size);
    return diff * PAGE_SIZE + it->start_addr;
}

void free_page(Page *p) noexcept
{
    if (!p)
        return;

    Page *next = nullptr;
    size_t num_of_pages = 0;

    if (p->type == Page::PageType::AllocatedPending) {
        next = p->pending_alloc_head.next;
        num_of_pages = p->pending_alloc_head.size_pages;
    } else if (p->type == Page::PageType::PendingFree) {
        num_of_pages = p->rcu_state.pages_to_free;
    } else {
        assert(!"Invalid page type");
    }

    // Coalesce region before the pages
    size_t coalesce_old_size = -1UL;
    auto region_before = p - 1;
    if (region_before->type == Page::PageType::Free) {
        p -= region_before->free_region_head.size;
        coalesce_old_size = region_before->free_region_head.size;
        p->free_region_head.size += num_of_pages;
    }

    // Coalesce region after the pages
    auto region_after = p + num_of_pages;
    if (region_after->type == Page::PageType::Free) {
        free_pages_list->remove(region_after);
        p->free_region_head.size += region_after->free_region_head.size;
    }

    // I am certainly not very good at naming variables
    if (coalesce_old_size == -1UL) {
        p->type = Page::PageType::Free;
        p->free_region_head.size = num_of_pages;
        auto order = log2(num_of_pages);
        if (order >= page_lists)
            order = page_lists - 1;
        free_pages_list[order].push_front(p);
    } else {
        auto old_order = log2(coalesce_old_size);
        int new_order;
        if (old_order < page_lists - 1 and old_order < (new_order = log2(p->free_region_head.size))) {
            if (new_order >= page_lists)
                new_order = page_lists - 1;
            
            free_pages_list[old_order].remove(p);
            free_pages_list[new_order].push_front(p);
        }
    }

    auto end = p + p->free_region_head.size - 1;
    end->type = Page::PageType::Free;
    end->free_region_head.size = p->free_region_head.size;

    free_page(next);
}

void Page::rcu_callback(void *ptr, bool) noexcept
{
    auto page = reinterpret_cast<Page *>((char *)ptr - offsetof(Page, rcu_state.rcu_h));
    // TODO: Free pages in chains

    free_page(page);
}

Page *find_page(Page::page_addr_t addr) noexcept
{
    if (phys_memory_regions.empty()) [[unlikely]]
        return nullptr;

    auto it = lower_bound(phys_memory_regions.begin(), phys_memory_regions.end(), addr,
                          [](const auto &a, const auto &b) { return a.start_addr < b; });

    if (it == phys_memory_regions.end())
        --it;

    if (it->start_addr <= addr && addr < it->start_addr + it->size) [[likely]] {
        auto diff = addr - it->start_addr;
        return it->pages + diff / PAGE_SIZE;
    }

    return nullptr;
}

Page_Descriptor Page_Descriptor::find_page_struct(Page::page_addr_t addr) noexcept
{
    auto p = find_page(addr);
    if (!p) [[unlikely]]
        return Page_Descriptor();

    assert(p->type == Page::PageType::Allocated);

    __atomic_add_fetch(&p->l.refcount, 1, __ATOMIC_SEQ_CST);
    return Page_Descriptor(p);
}

void kernel::pmm::free_memory_for_kernel(phys_page_t page, size_t number_of_pages) noexcept
{
    auto p = find_page(page);
    assert(p);

    p->type = Page::PageType::PendingFree;
    auto &&r         = p->rcu_state;
    r.pages_to_free  = number_of_pages;
    r.rcu_h.rcu_func = Page::rcu_callback;
    r.rcu_h.rcu_next = nullptr;

    get_cpu_struct()->paging_rcu_cpu.push(&r.rcu_h);
}

bool pmm_fully_initialized = false;
Page::page_addr_t alloc_pages_from_temp_pool(size_t pages) noexcept;

Page::page_addr_t kernel::pmm::get_memory_for_kernel(size_t pages) noexcept
{
    if (!pmm_fully_initialized) [[unlikely]]
        return alloc_pages_from_temp_pool(pages);

    Page *p = alloc_pages(pages, true);
    if (!p) [[unlikely]]
        return -1;

    for (size_t i = 0; i < pages; i++) {
        p[i].type       = Page::PageType::Allocated;
        p[i].flags      = 0;
        p[i].l.refcount = 1;
    }

    return phys_of_page(p);
}

Page::page_addr_t kernel::pmm::phys_of_page(Page *p) noexcept { return p->get_phys_addr(); }

Spinlock pmm_lock;

Page *kernel::pmm::alloc_pages(size_t count, bool /* contiguous */) noexcept
{
    assert(count > 0);

    Auto_Lock_Scope l(pmm_lock);

    if (phys_memory_regions.empty()) [[unlikely]]
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
    for (size_t i = order; i < page_lists; i++) {
        if (!free_pages_list[i].empty()) {
            auto p = &free_pages_list[i].front();
            if (p->free_region_head.size > count) {
                p->free_region_head.size -= count;
                auto t                   = p + p->free_region_head.size - 1;
                t->free_region_head.size = count;
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