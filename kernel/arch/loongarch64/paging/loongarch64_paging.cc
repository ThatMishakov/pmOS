#include "loongarch64_paging.hh"

#include "loong_temp_mapper.hh"

#include <kern_logger/kern_logger.hh>
#include <loongarch_asm.hh>
#include <loongarch_defs.hh>
#include <memory/pmm.hh>
#include <memory/temp_mapper.hh>
#include <pmos/containers/map.hh>
#include <utils.hh>

using namespace kernel;

constexpr u64 DIRECT_MAP_REGION = 0x8000000000000000;

unsigned valen = 48;

LoongArch64TempMapper temp_mapper;

u64 loongarch_cache_bits(Memory_Type t)
{
    switch (t) {
    case Memory_Type::Normal:
        return PAGE_MAT_CC;
    case Memory_Type::MemoryNoCache:
    case Memory_Type::IONoCache:
        return 0;
    }

    return 0;
}

void *LoongArch64TempMapper::kern_map(u64 phys_frame)
{
    return (void *)(phys_frame + DIRECT_MAP_REGION);
}
void LoongArch64TempMapper::return_map(void *) {}

Temp_Mapper &CPU_Info::get_temp_mapper() { return temp_mapper; }

void LoongArch64_Page_Table::apply() noexcept
{
    set_pgdl(page_directory);
    flush_tlb();
}

void invalidate_tlb_kernel(void *addr) { invalidate_kernel_page(addr, 0); }
void invalidate_tlb_kernel(void *addr, size_t size)
{
    for (u64 i = 0; i < size; i += PAGE_SIZE)
        invalidate_kernel_page(addr, 0);
}

constexpr u32 CSR_DMW0 = 0x180;

void set_dmws()
{
    asm volatile("csrwr %0, %1" ::"r"(DIRECT_MAP_REGION | (MAT_CC << 4) | 0x01), "i"(CSR_DMW0));
    asm volatile("csrwr %0, %1" ::"r"(0), "i"(CSR_DMW0 + 1));
    asm volatile("csrwr %0, %1" ::"r"(0), "i"(CSR_DMW0 + 2));
    asm volatile("csrwr %0, %1" ::"r"(0), "i"(CSR_DMW0 + 3));
}

void *LoongArch64_Page_Table::user_addr_max() const
{
    if (is_32bit())
        return (void *)(1UL << 32);
    else
        return (void *)(1UL << (valen - 1));
}

u64 kernel_page_dir = 0;

kresult_t map_kernel_page(u64 phys_addr, void *virt_addr, Page_Table_Argumments arg)
{
    return loongarch_map_page(kernel_page_dir, virt_addr, phys_addr, arg);
}

kresult_t unmap_kernel_page(TLBShootdownContext &ctx, void *virt_addr)
{
    return loongarch_unmap_page(ctx, kernel_page_dir, virt_addr, false);
}

kresult_t map_pages(ptable_top_ptr_t page_table, u64 phys_addr, void *virt_addr, size_t size,
                    Page_Table_Argumments arg)
{
    kresult_t result = 0;

    size_t i = 0;
    for (i = 0; i < size && (result == 0); i += 4096) {
        result = loongarch_map_page(page_table, (char *)virt_addr + i, phys_addr + i, arg);
    }

    // Leak memory on error
    if (result)
        serial_logger.printf("Warning: leaking memory in map_pages because of an error...\n");

    return result;
}

kresult_t loongarch_map_page(u64 pt_top_phys, void *virt_addr, u64 phys_addr,
                             Page_Table_Argumments arg)
{
    unsigned l4_idx          = ((u64)virt_addr >> paging_l4_offset) & page_idx_mask;
    unsigned l3_idx          = ((u64)virt_addr >> paging_l3_offset) & page_idx_mask;
    unsigned l2_idx          = ((u64)virt_addr >> paging_l2_offset) & page_idx_mask;
    unsigned l1_idx          = ((u64)virt_addr >> paging_l1_offset) & page_idx_mask;
    unsigned priviledge_bits = arg.user_access ? PAGE_USER_MASK : 0;

    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    u64 *l4_pt = mapper.map(pt_top_phys);
    auto pte   = __atomic_load_n(l4_pt + l4_idx, __ATOMIC_RELAXED);
    if (!(pte & PAGE_VALID)) {
        u64 new_pt_phys = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(new_pt_phys))
            return -ENOMEM;

        clear_page(new_pt_phys);
        pte = new_pt_phys | PAGE_VALID | priviledge_bits;
        __atomic_store_n(l4_pt + l4_idx, pte, __ATOMIC_RELAXED);
    }

    u64 *l3_pt = mapper.map(pte & PAGE_ADDR_MASK);
    pte        = __atomic_load_n(l3_pt + l3_idx, __ATOMIC_RELAXED);
    if (!(pte & PAGE_VALID)) {
        u64 new_pt_phys = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(new_pt_phys))
            return -ENOMEM;

        clear_page(new_pt_phys);
        pte = new_pt_phys | PAGE_VALID | priviledge_bits;
        __atomic_store_n(l3_pt + l3_idx, pte, __ATOMIC_RELAXED);
    }

    u64 *l2_pt = mapper.map(pte & PAGE_ADDR_MASK);
    pte        = __atomic_load_n(l2_pt + l2_idx, __ATOMIC_RELAXED);
    if (!(pte & PAGE_VALID)) {
        u64 new_pt_phys = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(new_pt_phys))
            return -ENOMEM;

        clear_page(new_pt_phys);
        pte = new_pt_phys | PAGE_VALID | priviledge_bits;
        __atomic_store_n(l2_pt + l2_idx, pte, __ATOMIC_RELAXED);
    }

    u64 *l1_pt = mapper.map(pte & PAGE_ADDR_MASK);
    pte        = __atomic_load_n(l1_pt + l1_idx, __ATOMIC_RELAXED);

    if (pte & PAGE_VALID)
        return -EEXIST;

    pte = phys_addr;
    pte |= PAGE_VALID;
    pte |= priviledge_bits;
    pte |= arg.extra << PAGE_AVAILABLE_SHIFT;
    pte |= loongarch_cache_bits(arg.cache_policy);
    if (!arg.readable)
        pte |= PAGE_NO_READ;
    if (arg.writeable)
        pte |= PAGE_WRITEABLE;
    if (arg.execution_disabled)
        pte |= PAGE_NO_EXECUTE;

    __atomic_store_n(l1_pt + l1_idx, pte, __ATOMIC_RELEASE);
    return 0;
}

klib::shared_ptr<LoongArch64_Page_Table> LoongArch64_Page_Table::create_empty(int flags)
{
    klib::shared_ptr<LoongArch64_Page_Table> new_table =
        klib::unique_ptr<LoongArch64_Page_Table>(new LoongArch64_Page_Table(flags));

    auto n = pmm::get_memory_for_kernel(1);
    if (pmm::alloc_failure(new_table->page_directory))
        return nullptr;

    new_table->page_directory = n;

    clear_page(new_table->page_directory);

    auto result = insert_global_page_tables(new_table);

    if (result) {
        pmm::free_memory_for_kernel(new_table->page_directory, 1);
        new_table->page_directory = PAGE_DIRECTORY_INITIALIZER;
    }

    return result ? nullptr : new_table;
}

LoongArch64_Page_Table::~LoongArch64_Page_Table()
{
    if (page_directory != PAGE_DIRECTORY_INITIALIZER) {
        free_all_pages();
        pmm::free_memory_for_kernel(page_directory, 1);
    }

    takeout_global_page_tables();
}

kresult_t LoongArch64_Page_Table::map(u64 page_addr, void *virt_addr, Page_Table_Argumments arg)
{
    return loongarch_map_page(page_directory, virt_addr, page_addr, arg);
}

kresult_t LoongArch64_Page_Table::map(pmm::Page_Descriptor page, void *virt_addr,
                                      Page_Table_Argumments arg)
{
    auto page_phys = page.get_phys_addr();
    arg.extra      = PAGING_FLAG_STRUCT_PAGE;
    auto result    = loongarch_map_page(page_directory, virt_addr, page_phys, arg);
    if (result == 0)
        page.takeout_page();
    return result;
}

pmos::containers::map<u64, klib::shared_ptr<LoongArch64_Page_Table>> page_tables;
Spinlock page_tables_lock;

kresult_t
    LoongArch64_Page_Table::insert_global_page_tables(klib::shared_ptr<LoongArch64_Page_Table> t)
{
    assert(t);
    auto id = t->id;
    Auto_Lock_Scope l(page_tables_lock);
    return page_tables.insert_noexcept({id, std::move(t)}).second ? 0 : -ENOMEM;
}

void LoongArch64_Page_Table::takeout_global_page_tables()
{
    Auto_Lock_Scope l(page_tables_lock);
    page_tables.erase(id);
}

void apply_page_table(ptable_top_ptr_t page_table)
{
    set_pgdh(page_table);
    flush_tlb();
}

static void free_leaf_pte(u64 pte)
{
    if (!(pte & PAGE_VALID))
        return;

    u64 page_phys = pte & PAGE_ADDR_MASK;

    if (!(pte & PAGE_AVAILABLE_MASK))
        pmm::free_memory_for_kernel(page_phys, 1);
    else if ((pte >> PAGE_AVAILABLE_SHIFT) & PAGING_FLAG_STRUCT_PAGE) {
        auto p = pmm::Page_Descriptor::find_page_struct(page_phys);
        assert(p.page_struct_ptr && "page struct must be present");
        p.release_taken_out_page();
    }
}

LoongArch64_Page_Table::Page_Info LoongArch64_Page_Table::get_page_mapping(void *virt_addr) const
{
    Page_Info i {};

    unsigned l4_idx = ((u64)virt_addr >> paging_l4_offset) & page_idx_mask;
    unsigned l3_idx = ((u64)virt_addr >> paging_l3_offset) & page_idx_mask;
    unsigned l2_idx = ((u64)virt_addr >> paging_l2_offset) & page_idx_mask;
    unsigned l1_idx = ((u64)virt_addr >> paging_l1_offset) & page_idx_mask;

    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    u64 *l4_pt = mapper.map(page_directory);
    auto pte   = __atomic_load_n(l4_pt + l4_idx, __ATOMIC_RELAXED);
    if (!(pte & PAGE_VALID))
        return i;

    u64 *l3_pt = mapper.map(pte & PAGE_ADDR_MASK);
    pte        = __atomic_load_n(l3_pt + l3_idx, __ATOMIC_RELAXED);
    if (!(pte & PAGE_VALID))
        return i;

    u64 *l2_pt = mapper.map(pte & PAGE_ADDR_MASK);
    pte        = __atomic_load_n(l2_pt + l2_idx, __ATOMIC_RELAXED);
    if (!(pte & PAGE_VALID))
        return i;

    u64 *l1_pt = mapper.map(pte & PAGE_ADDR_MASK);
    pte        = __atomic_load_n(l1_pt + l1_idx, __ATOMIC_RELAXED);
    if (!(pte & PAGE_VALID))
        return i;

    i.is_allocated = !!(pte & PAGE_VALID);
    i.dirty        = !!(pte & PAGE_DIRTY);
    i.flags        = (pte >> 8) & 0xf;
    i.user_access  = !!(pte & PAGE_USER_MASK);
    i.writeable    = !!(pte & PAGE_WRITEABLE);
    i.executable   = !(pte & PAGE_NO_EXECUTE);
    i.nofree       = !!(i.flags & PAGING_FLAG_NOFREE);
    i.page_addr    = pte & PAGE_ADDR_MASK;

    return i;
}

static void free_all_level(u64 level_page, unsigned level)
{
    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    u64 *active_pt = mapper.map(level_page);

    for (unsigned i = 0; i < 512; ++i) {
        auto pte = __atomic_load_n(active_pt + i, __ATOMIC_RELAXED);
        if (!(pte & PAGE_VALID))
            continue;

        if (level == 1) {
            free_leaf_pte(pte);
        } else {
            u64 page = pte & PAGE_ADDR_MASK;
            free_all_level(page, level - 1);
            pmm::free_memory_for_kernel(page, 1);
        }
    }
}

void LoongArch64_Page_Table::free_all_pages() { free_all_level(page_directory, 4); }

void LoongArch64_Page_Table::invalidate_tlb(void *page) { invalidate_user_page(page, 0); }

void LoongArch64_Page_Table::invalidate_tlb(void *page, size_t size)
{
    if (size / 0x1000 > 128) {
        flush_user_pages();
        return;
    }

    for (char *i = (char *)page; i < (char *)page + size; i += 0x1000) {
        invalidate_user_page(i, 0);
    }
}

void LoongArch64_Page_Table::tlb_flush_all() { flush_user_pages(); }

void LoongArch64_Page_Table::invalidate(TLBShootdownContext &ctx, void *virt_addr,
                                        bool free) noexcept
{
    loongarch_unmap_page(ctx, page_directory, virt_addr, free);
}