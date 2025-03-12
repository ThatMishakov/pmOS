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

Memory_Type pte_cache_policy(u64 e)
{
    switch (e & 0x30) {
    case PAGE_MAT_CC:
        return Memory_Type::Normal;
        // TODO: Framebuffer...
    default:
        return Memory_Type::IONoCache;
    }
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

void LoongArch64_Page_Table::invalidate_range(TLBShootdownContext &ctx, void *virt_addr,
                                              size_t size_bytes, bool free)
{
    // Slow but doesn't matter for now
    char *end = (char *)virt_addr + size_bytes;
    for (char *i = (char *)virt_addr; i < end; i += 4096)
        invalidate(ctx, i, free);
}

kresult_t LoongArch64_Page_Table::resolve_anonymous_page(void *virt_addr, unsigned access_type)
{
    assert(access_type & Writeable);

    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    mapper.map(page_directory);
    for (int i = 4; i > 1; --i) {
        const u8 offset = 12 + (i - 1) * 9;
        const u64 index = ((u64)virt_addr >> offset) & 0x1FF;

        u64 entry = __atomic_load_n(mapper.ptr + index, __ATOMIC_RELAXED);
        assert(entry & PAGE_VALID);
        mapper.map(entry & PAGE_ADDR_MASK);
    }

    const u64 index = ((u64)virt_addr >> 12) & 0x1FF;
    u64 entry       = __atomic_load_n(mapper.ptr + index, __ATOMIC_RELAXED);
    assert(entry & PAGE_VALID);
    assert(entry & (PAGING_FLAG_STRUCT_PAGE << PAGE_AVAILABLE_SHIFT));
    assert(!(entry & PAGE_WRITEABLE));

    auto page = pmm::Page_Descriptor::find_page_struct(entry & PAGE_ADDR_MASK);
    assert(page.page_struct_ptr);

    if (__atomic_load_n(&page.page_struct_ptr->l.refcount, __ATOMIC_ACQUIRE) == 2) {
        // only owner of the page
        entry = PAGE_WRITEABLE;
        __atomic_store_n(mapper.ptr + index, entry, __ATOMIC_RELEASE);
        invalidate_user_page((void *)virt_addr, 0);
        return 0;
    }

    auto owner = page.page_struct_ptr->l.owner;
    assert(owner && "page owner not found");

    auto new_descriptor =
        owner->atomic_request_anonymous_page(page.page_struct_ptr->l.offset, true);
    if (!new_descriptor.success())
        return new_descriptor.result;

    entry &= ~PAGE_VALID;
    __atomic_store_n(mapper.ptr + index, entry, __ATOMIC_RELEASE);

    {
        auto tlb_ctx = TLBShootdownContext::create_userspace(*this);
        tlb_ctx.invalidate_page(virt_addr);
    }

    u64 new_page_phys = new_descriptor.val.takeout_page();

    Temp_Mapper_Obj<u64> new_mapper(request_temp_mapper());
    void *new_page = new_mapper.map(new_page_phys);
    Temp_Mapper_Obj<u64> old_mapper(request_temp_mapper());
    void *old_page = old_mapper.map(entry & PAGE_ADDR_MASK);

    memcpy(new_page, old_page, PAGE_SIZE);

    page.release_taken_out_page();

    entry |= PAGE_VALID;
    entry |= PAGE_WRITEABLE;
    entry &= ~PAGE_ADDR_MASK;
    entry |= new_page_phys;
    __atomic_store_n(mapper.ptr + index, entry, __ATOMIC_RELEASE);

    invalidate_user_page((void *)virt_addr, 0);
    return 0;
}

kresult_t loongarch_unmap_page(TLBShootdownContext &ctx, u64 pt_top_phys, void *virt_addr,
                               bool free)
{

    unsigned l4_idx = ((u64)virt_addr >> paging_l4_offset) & page_idx_mask;
    unsigned l3_idx = ((u64)virt_addr >> paging_l3_offset) & page_idx_mask;
    unsigned l2_idx = ((u64)virt_addr >> paging_l2_offset) & page_idx_mask;
    unsigned l1_idx = ((u64)virt_addr >> paging_l1_offset) & page_idx_mask;

    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());

    u64 *l4_pt   = mapper.map(pt_top_phys);
    u64 l4_entry = __atomic_load_n(l4_pt + l4_idx, __ATOMIC_RELAXED);
    if (!(l4_entry & PAGE_VALID))
        return -ENOENT;

    u64 *l3_pt   = mapper.map(l4_entry & PAGE_ADDR_MASK);
    u64 l3_entry = __atomic_load_n(l3_pt + l3_idx, __ATOMIC_RELAXED);
    if (!(l3_entry & PAGE_VALID))
        return -ENOENT;

    u64 *l2_pt   = mapper.map(l3_entry & PAGE_ADDR_MASK);
    u64 l2_entry = __atomic_load_n(l2_pt + l2_idx, __ATOMIC_RELAXED);
    if (!(l2_entry & PAGE_VALID))
        return -ENOENT;

    u64 *l1_pt   = mapper.map(l2_entry & PAGE_ADDR_MASK);
    u64 l1_entry = __atomic_load_n(l1_pt + l1_idx, __ATOMIC_RELAXED);
    if (!(l1_entry & PAGE_VALID))
        return -ENOENT;

    __atomic_store_n(l1_pt + l1_idx, 0, __ATOMIC_RELEASE);

    if (free)
        free_leaf_pte(l1_entry);

    ctx.invalidate_page(virt_addr);
    return 0;
}

static kresult_t copy_to_recursive(const klib::shared_ptr<Page_Table> &to, u64 phys_page_level,
                                   u64 absolute_start, u64 to_addr, u64 size_bytes, u64 new_access,
                                   u64 current_copy_from, int level, u64 &upper_bound_offset,
                                   TLBShootdownContext &ctx)
{
    const u8 offset = 12 + (level - 1) * 9;

    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    mapper.map(phys_page_level);

    u64 mask        = (1UL << offset) - 1;
    u64 max         = (absolute_start + size_bytes + mask) & ~mask;
    u64 end_index   = (current_copy_from >> (offset + 9)) == (max >> (offset + 9))
                          ? (max >> offset) & 0x1ff
                          : 512;
    u64 start_index = (current_copy_from >> offset) & 0x1ff;

    for (u64 i = start_index; i < end_index; ++i) {
        auto pte = __atomic_load_n(mapper.ptr + i, __ATOMIC_RELAXED);
        if (!(pte & PAGE_PRESENT))
            continue;

        if (level == 1) {
            auto p = pmm::Page_Descriptor::find_page_struct(pte & PAGE_ADDR_MASK);
            assert(p.page_struct_ptr && "page struct must be present");
            if (!p.page_struct_ptr->is_anonymous())
                continue;

            u64 copy_from = ((i - start_index) << offset) + current_copy_from;
            u64 copy_to   = copy_from - absolute_start + to_addr;

            if (pte & PAGE_WRITEABLE) {
                pte &= ~PAGE_WRITEABLE;
                __atomic_store_n(mapper.ptr + i, pte, __ATOMIC_RELEASE);
                ctx.invalidate_page((void *)copy_from);
            }

            Page_Table_Argumments arg = {
                .readable           = !!(new_access & Readable),
                .writeable          = 0,
                .user_access        = true,
                .global             = false,
                .execution_disabled = not(new_access & Executable),
                .extra              = static_cast<u8>(pte >> PAGE_AVAILABLE_SHIFT),
                .cache_policy       = pte_cache_policy(pte),
            };
            auto result = to->map(klib::move(p), (void *)copy_to, arg);
            if (result)
                return result;

            upper_bound_offset = copy_from - absolute_start + 4096;
        } else {
            u64 next_level_phys = pte & PAGE_USER_MASK;
            u64 current         = ((i - start_index) << offset) + current_copy_from;
            if (i != start_index)
                current &= ~mask;
            auto result =
                copy_to_recursive(to, next_level_phys, absolute_start, to_addr, size_bytes,
                                  new_access, current, level - 1, upper_bound_offset, ctx);
            if (result)
                return result;
        }
    }

    return 0;
}

kresult_t LoongArch64_Page_Table::copy_anonymous_pages(const klib::shared_ptr<Page_Table> &to,
                                                       void *from_addr, void *to_addr,
                                                       size_t size_bytes, unsigned access)
{
    u64 offset = 0;
    kresult_t result;
    {
        TLBShootdownContext ctx = TLBShootdownContext::create_userspace(*this);
        result = copy_to_recursive(to, page_directory, (u64)from_addr, (u64)to_addr, size_bytes,
                                   access, (u64)from_addr, 4, offset, ctx);
    }

    if (result != 0) {
        auto ctx = TLBShootdownContext::create_userspace(*to);
        to->invalidate_range(ctx, (void *)to_addr, offset, true);
    }

    return result;
}