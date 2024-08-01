#include "pmm.hh"

#include "bsearch.hh"

#include <sched/sched.hh>

using namespace kernel;
using namespace kernel::pmm;

RegionsTree::RBTreeHead kernel::pmm::memory_regions;
klib::vector<PageArrayDescriptor> kernel::pmm::phys_memory_regions;

Page::page_addr_t Page::get_phys_addr() const noexcept
{
    assert(has_physical_page());

    auto it = memory_regions.lower_bound(this);
    assert(it);

    auto diff = this - it->pages;
    assert(diff < (long int)it->size);
    return diff * PAGE_SIZE + it->start_addr;
}

void free_page(Page *p) noexcept;

void Page::rcu_callback(void *ptr, bool) noexcept
{
    auto page = reinterpret_cast<Page *>((char *)ptr - offsetof(Page, rcu_state.rcu_h));
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

    auto &&r = p->rcu_state;
    r.pages_to_free = number_of_pages;
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
        p[i].type = Page::PageType::Allocated;
        p[i].flags = 0;
        p[i].l.refcount = 1;
    }

    return phys_of_page(p);
}

Page::page_addr_t kernel::pmm::phys_of_page(Page *p) noexcept
{
    return p->get_phys_addr();
}