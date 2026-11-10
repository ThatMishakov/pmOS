#include "aarch64_paging.hh"
#include <types.hh>
#include <arm_asm.hh>
#include <pmos/utility/scope_guard.hh>
#include <kern_logger/kern_logger.hh>

using namespace kernel::aarch64::paging;
using namespace kernel::paging;
using namespace kernel;

static constexpr unsigned MT_NORMAL = 0;
static constexpr unsigned MT_NORMAL_NC = 1;
static constexpr unsigned MT_DEVICE_nGnRnE = 2;
static constexpr unsigned MT_DEVICE_nGnRE = 3;

static constexpr u64 MAIR_NORMAL = 0xff;
static constexpr u64 MAIR_NORMAL_NC = 0x44;
static constexpr u64 MAIR_DEVICE_nGnRnE = 0x00;
static constexpr u64 MAIR_DEVICE_nGnRE = 0x04;


static constexpr u64 DESCR_PRESENT = 1 << 0;
static constexpr u64 DESCR_TABLE = 1 << 1;
static constexpr u64 DESCR_INNER_SHAREABLE = 3 << 8;
static constexpr u64 DESCR_ACCESS = 1 << 10;

static constexpr u64 DESCR_PXN = (u64)1 << 53;
static constexpr u64 DESCR_UXN = (u64)1 << 54;

u64 max_phys_addr = 0;

phys_addr_t kernel::paging::arch_phys_addr_limit()
{
    return max_phys_addr;
}

bool AArch64_Page_Table::is_32bit() const
{
    return flags & FLAG_32BIT;
}

u64 kernel_page_table = 0;

// Preface: ARM ARM refers to levels as -1 to 3, here the levels are 4 to 0.

namespace {

constexpr u64 mair_attridx(u64 attr, unsigned idx)
{
    return attr << idx * 8;
}

unsigned paging_levels = 0;
// paging_levels - 1
unsigned top_level = 0;
unsigned page_table_mask = 0;
unsigned top_level_mask = 0;
unsigned va_bits = 0;

unsigned page_ptr_idx(void *virt_addr, unsigned level)
{
    u64 addr = reinterpret_cast<u64>(virt_addr);
    addr >>= page_order + ((page_order - 3) * level);
    if (level == top_level)
        return addr & top_level_mask;
    else
        return addr & page_table_mask;
}

u64 load_pte(u64 *table, unsigned idx)
{
    return __atomic_load_n(table + idx, __ATOMIC_RELAXED);
}
void store_pte(u64 *table, unsigned idx, u64 value)
{
    __atomic_store_n(table + idx, value, __ATOMIC_RELAXED);
}

constexpr u64 type_to_attridx(Memory_Type type)
{
    u64 attr = 0;
    switch (type) {
    case Memory_Type::Normal:
        attr = MT_NORMAL;
        break;
    case Memory_Type::MemoryNoCache:
        attr = MT_NORMAL_NC;
        break;
    case Memory_Type::IONoCache:
        attr = MT_DEVICE_nGnRnE;
        break;
    case Memory_Type::Framebuffer:
        attr = MT_DEVICE_nGnRE;
        break;
    default:
        assert(false && "Invalid memory type");
        break;
    }

    return attr << 2;
}
Memory_Type pte_to_type(u64 attridx)
{
    attridx >>= 2;
    attridx &= 0x7;

    switch (attridx) {
    case MT_NORMAL:
        return Memory_Type::Normal;
    case MT_NORMAL_NC:
        return Memory_Type::MemoryNoCache;
    case MT_DEVICE_nGnRnE:
        return Memory_Type::IONoCache;
    case MT_DEVICE_nGnRE:
        return Memory_Type::Framebuffer;
    default:
        assert(false && "Invalid memory type");
        return Memory_Type::Normal;
    }
}

constexpr u64 arg_to_ap(Page_Table_Arguments arg)
{
    u64 ap = 0;
    if (arg.user_access)
        ap |= 1 << 6;
    if (!arg.writeable)
        ap |= 1 << 7;

    if (arg.execution_disabled)
        ap |= DESCR_PXN | DESCR_UXN;
    else if (arg.user_access)
        ap |= DESCR_PXN;
    else
        ap |= DESCR_UXN;

    return ap;
}

u64 extra_to_flags(u8 extra)
{
    return u64(extra & 0x3) << 55;
}
u8 flags_to_extra(u64 flags)
{
    return (flags >> 55) & 0x3;
}

u64 phys_addr_to_pte(u64 phys_addr)
{
    if (page_order == 16)
        return (phys_addr & 0x0000ffffffff0000) | (phys_addr >> 36 & 0xf000); 
    else
        return (phys_addr & 0x0003fffffffff000) | (phys_addr >> 42 & 0x300);
}
u64 pte_to_phys_addr(u64 pte)
{
    if (page_order == 16)
        return (pte & 0x0000ffffffff0000) | ((pte & 0xf000) << 36);
    else
        return (pte & 0x0003fffffffff000) | ((pte & 0x300) << 42);
}

u64 construct_pte(u64 phys_addr, Page_Table_Arguments arg, bool block = false)
{
    u64 pte = phys_addr_to_pte(phys_addr);
    pte |= DESCR_PRESENT;
    if (!block)
        pte |= DESCR_TABLE;
    pte |= type_to_attridx(arg.cache_policy);
    pte |= arg_to_ap(arg);
    pte |= DESCR_INNER_SHAREABLE;
    pte |= DESCR_ACCESS;
    pte |= extra_to_flags(arg.extra);

    return pte;
}

u64 construct_table(u64 phys_addr, Page_Table_Arguments arg)
{
    u64 pte = phys_addr_to_pte(phys_addr);
    pte |= DESCR_PRESENT;
    pte |= DESCR_TABLE;

    if (arg.user_access)
        pte |= DESCR_PXN;
    else
        pte |= DESCR_UXN;

    return pte;
}

Page_Info decode_pte(u64 pte, unsigned level)
{
    bool is_block = level != 0;

    Page_Info i {};
    i.is_allocated = !!(pte & DESCR_PRESENT);
    i.invalid       = is_block == !(pte & DESCR_TABLE);
    i.user_access   = !!(pte & (1 << 6));
    i.writeable     = !(pte & (1 << 7));
    i.readable      = i.is_allocated;
    i.executable    = !(pte & DESCR_PXN) && !(pte & DESCR_UXN);
    i.cache_policy  = pte_to_type(pte);
    i.flags         = flags_to_extra(pte);
    i.page_addr     = pte_to_phys_addr(pte);
    i.nofree        = !!(i.flags & PAGING_FLAG_NOFREE);
    i.page_addr     = pte_to_phys_addr(pte);

    // TODO: Check reserved bits here for the invalid bit...

    return i;
}

result_t map_page_recursive(u64 page_table_addr, u64 phys_addr, void *virt_addr, ::kernel::paging::Page_Table_Arguments arg, unsigned level)
{
    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    u64 *page_table = mapper.map(page_table_addr);
    auto idx = page_ptr_idx(virt_addr, level);
    auto pte = load_pte(page_table, idx);

    if (level == 0) {
        if (pte & DESCR_PRESENT) {
            return -EEXIST;
        }

        pte = construct_pte(phys_addr, arg);
        store_pte(page_table, idx, pte);
        dsb<Barrier::ISHST>();
        return 0;
    } else {
        if (!(pte & DESCR_PRESENT)) {
            auto new_table = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_table))
                return -ENOMEM;

            clear_page(new_table);
            dmb<Barrier::ISHST>();

            pte = construct_table(new_table, arg);
            store_pte(page_table, idx, pte);
            dsb<Barrier::ISHST>();
        }
        if ((pte & DESCR_TABLE) == 0) {
            // Huge page
            return -EEXIST;
        }

        auto next_table = pte_to_phys_addr(pte);
        return map_page_recursive(next_table, phys_addr, virt_addr, arg, level - 1);
    }
}

Page_Info get_mapping_recursive(u64 page_table_addr, void *virt_addr, unsigned level)
{
    Page_Info i {};
    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    u64 *page_table = mapper.map(page_table_addr);
    auto idx = page_ptr_idx(virt_addr, level);
    auto pte = load_pte(page_table, idx);

    if (!(pte & DESCR_PRESENT)) {
        return i;
    }

    if (level == 0) {
        return decode_pte(pte, level);
    } else {
        if (!(pte & DESCR_TABLE)) {
            // Block descriptor
            return decode_pte(pte, level);
        }

        auto next_table = pte_to_phys_addr(pte);
        return get_mapping_recursive(next_table, virt_addr, level - 1);
    }
}

unsigned entries_for_level(unsigned level)
{
    if (level == top_level)
        return 1 << (va_bits - page_order - (page_order - 3) * level);
    else
        return 1 << (page_order - 3);
}

void free_level(u64 page_table_addr, unsigned level)
{
    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    u64 *page_table = mapper.map(page_table_addr);

    auto entries = entries_for_level(level);
    for (unsigned i = 0; i < entries; i++) {
        auto pte = load_pte(page_table, i);
        if (!(pte & DESCR_PRESENT))
            continue;

        store_pte(page_table, i, 0);
        dsb<Barrier::ISHST>();

        if (level != 0 && (pte & DESCR_TABLE)) {
            auto next_table = pte_to_phys_addr(pte);
            free_level(next_table, level - 1);
            pmm::free_memory_for_kernel(next_table, 1);
        } else {
            auto extra_flags = flags_to_extra(pte);
            if (extra_flags & PAGING_FLAG_NOFREE)
                continue;

            assert(level == 0);

            auto page_addr = pte_to_phys_addr(pte);
            if (extra_flags & PAGING_FLAG_STRUCT_PAGE) {
                auto p = pmm::Page_Descriptor::find_page_struct(page_addr);
                assert(p.page_struct_ptr && "page struct must be present");
                p.release_taken_out_page();
            } else {
                pmm::free_memory_for_kernel(page_addr, 1);
            }
        }
    }
}

void invalidate_level(::kernel::paging::TLBShootdownContext& ctx, u64 page_table, void *virt_addr_start, size_t size_bytes, bool free, unsigned level)
{
    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    u64 *active_pt = mapper.map(page_table);
    const ulong level_size = 1UL << (page_order + (page_order - 3) * level);
    const ulong level_mask = level_size - 1;

    const ulong end = reinterpret_cast<ulong>(virt_addr_start) + size_bytes;

    const ulong start_aligned = reinterpret_cast<ulong>(virt_addr_start) & ~level_mask;
    const ulong end_aligned = (reinterpret_cast<ulong>(virt_addr_start) + size_bytes + level_mask) & ~level_mask;

    const unsigned start_idx = page_ptr_idx((void *)start_aligned, level);
    const unsigned end_idx = start_idx + (end_aligned - start_aligned) / level_size;

    for (unsigned i = start_idx; i < end_idx; i++) {
        auto pte = load_pte(active_pt, i);
        if (!(pte & DESCR_PRESENT))
            continue;

        auto phys_addr = pte_to_phys_addr(pte);

        if (level != 0 && (pte & DESCR_TABLE)) {
            ulong level_start = start_aligned + (i - start_idx) * level_size;
            ulong level_end = level_start + level_size;

            ulong sub_start = std::max(level_start, reinterpret_cast<ulong>(virt_addr_start));
            ulong sub_end = end == 0 || level_end < end ? level_end : end;

            invalidate_level(ctx, phys_addr, (void *)sub_start, sub_end - sub_start, free, level - 1);
        } else {
            store_pte(active_pt, i, 0);
            dsb<Barrier::ISHST>();
            ctx.invalidate_page((void *)(start_aligned + (i - start_idx) * level_size));
            
            auto extra_flags = flags_to_extra(pte);
            if (free && !(extra_flags & PAGING_FLAG_NOFREE)) {
                assert(level == 0);

                if (extra_flags & PAGING_FLAG_STRUCT_PAGE) {
                    auto p = pmm::Page_Descriptor::find_page_struct(phys_addr);
                    assert(p.page_struct_ptr && "page struct must be present");
                    p.release_taken_out_page();
                } else {
                    pmm::free_memory_for_kernel(phys_addr, 1);
                }
            }
        }
    }
}

void invalidate_pages(::kernel::paging::TLBShootdownContext& ctx, u64 page_table, void *virt_addr, size_t size_bytes, bool free)
{
    return invalidate_level(ctx, page_table, virt_addr, size_bytes, free, top_level);
}

} // namespace

void init_paging(unsigned va_bits)
{
    assert(page_order == 12 || page_order == 14 || page_order == 16);
    assert(va_bits >= 32 && va_bits <= 52);

    const unsigned index_bits = page_order - 3;

    ::va_bits = va_bits;

    paging_levels = (va_bits - page_order + index_bits - 1) / index_bits;
    top_level = paging_levels - 1;

    page_table_mask = (1 << index_bits) - 1;
    
    const unsigned top_level_shift =
        page_order + top_level * index_bits;
    const unsigned top_level_bits = va_bits - top_level_shift;

    top_level_mask = (1U << top_level_bits) - 1;
}

kresult_t kernel::paging::map_page(u64 page_table, u64 phys_addr, void *virt_addr, ::kernel::paging::Page_Table_Arguments arg)
{
    return map_page_recursive(page_table, phys_addr, virt_addr, arg, top_level);
}
kresult_t kernel::paging::map_pages(ptable_top_ptr_t page_table, phys_addr_t phys_addr, void *virt_addr, size_t size_bytes, kernel::paging::Page_Table_Arguments arg)
{
    kresult_t result = 0;

    size_t i = 0;
    for (i = 0; i < size_bytes && (result == 0); i += PAGE_SIZE) {
        result = map_page_recursive(page_table, phys_addr + i, (void *)((char *)virt_addr + i), arg, top_level);
    }

    // Leak memory on error
    if (result)
        log::serial_logger.printf("Warning: leaking memory in map_pages because of an error...\n");

    return result;
}

kresult_t kernel::paging::map_kernel_page(phys_addr_t phys_addr, void *virt_addr, kernel::paging::Page_Table_Arguments arg)
{
    return map_page(kernel_page_table, phys_addr, virt_addr, arg);
}

kresult_t kernel::paging::map_kernel_pages(phys_addr_t phys_addr, void *virt_addr, size_t size, kernel::paging::Page_Table_Arguments arg)
{
    return map_pages(kernel_page_table, phys_addr, virt_addr, size, arg);
}

Page_Info AArch64_Page_Table::get_page_mapping(void *virt_addr) const
{
    assert(page_directory != PAGE_DIRECTORY_INITIALIZER);
    return get_mapping_recursive(page_directory, virt_addr, top_level);
}

void AArch64_Page_Table::invalidate(::kernel::paging::TLBShootdownContext& ctx, void *virt_addr, bool free) noexcept
{
    assert(page_directory != PAGE_DIRECTORY_INITIALIZER);
    invalidate_pages(ctx, page_directory, virt_addr, PAGE_SIZE, free);
}

void AArch64_Page_Table::invalidate_range(::kernel::paging::TLBShootdownContext& ctx, void *virt_addr, size_t size_bytes, bool free)
{
    assert(page_directory != PAGE_DIRECTORY_INITIALIZER);
    invalidate_pages(ctx, page_directory, virt_addr, size_bytes, free);
}

kresult_t paging::unmap_kernel_page(kernel::paging::TLBShootdownContext& ctx, void* virt_addr, bool free)
{
    invalidate_pages(ctx, kernel_page_table, virt_addr, PAGE_SIZE, free);
    return 0;
}

klib::shared_ptr<AArch64_Page_Table> AArch64_Page_Table::create_empty(int flags)
{
    auto new_table = klib::make_shared<AArch64_Page_Table>();
    if (!new_table)
        return nullptr;

    new_table->flags = flags;

    auto n = pmm::get_memory_for_kernel(1);
    if (pmm::alloc_failure(n))
        return nullptr;

    new_table->page_directory = n;

    clear_page(new_table->page_directory);

    auto result = insert_global_page_tables(new_table);
    return result ? nullptr : new_table;
}

klib::shared_ptr<AArch64_Page_Table> AArch64_Page_Table::create_clone()
{
    klib::shared_ptr<AArch64_Page_Table> new_table = create_empty();
    if (!new_table)
        return nullptr;

    Auto_Lock_Scope_Double scope_guard(this->lock, new_table->lock);

    if (!new_table->paging_regions.empty())
        return nullptr; // "page table is already cloned"

    // This gets called on error
    auto guard = pmos::utility::make_scope_guard([&]() {
        auto tlb_ctx = TLBShootdownContext::create_userspace(*new_table);

        auto it = new_table->paging_regions.begin();
        while (it != new_table->paging_regions.end()) {
            auto region_start = it->start_addr;
            auto region_size  = it->size;

            it->prepare_deletion();
            new_table->paging_regions.erase(it);

            new_table->invalidate_range(tlb_ctx, region_start, region_size, true);

            it = new_table->paging_regions.begin();
        }
    });

    free_all_pages();
    {
        auto ctx = TLBShootdownContext::create_userspace(*this);
        ctx.invalidate_all();
    }

    for (auto &reg: this->paging_regions) {
        auto result = reg.clone_to(new_table, reg.start_addr, reg.access_type);
        if (result)
            return nullptr;
    }

    guard.dismiss();

    return new_table;
}

kresult_t AArch64_Page_Table::map(u64 page_addr, void *virt_addr, ::kernel::paging::Page_Table_Arguments arg)
{
    return map_page(page_directory, page_addr, virt_addr, arg);
}

pmos::containers::map<u64, klib::shared_ptr<AArch64_Page_Table>> page_tables;
Spinlock page_tables_lock;

kresult_t
    AArch64_Page_Table::insert_global_page_tables(klib::shared_ptr<AArch64_Page_Table> t)
{
    assert(t);
    auto id = t->id;
    Auto_Lock_Scope l(page_tables_lock);
    return page_tables.insert_noexcept({id, std::move(t)}).second ? 0 : -ENOMEM;
}

void AArch64_Page_Table::takeout_global_page_tables()
{
    Auto_Lock_Scope l(page_tables_lock);
    page_tables.erase(id);
}

klib::shared_ptr<AArch64_Page_Table> AArch64_Page_Table::get_page_table(u64 id) noexcept
{
    auto it = page_tables.find(id);
    return it == page_tables.end() ? nullptr : it->second;
}

void AArch64_Page_Table::free_all_pages()
{
    assert(page_directory != PAGE_DIRECTORY_INITIALIZER);
    free_level(page_directory, top_level);
}

AArch64_Page_Table::~AArch64_Page_Table()
{
    if (page_directory != PAGE_DIRECTORY_INITIALIZER) {
        free_all_pages();
        pmm::free_memory_for_kernel(page_directory, 1);
    }

    takeout_global_page_tables();
}

void *AArch64_Page_Table::user_addr_max() const
{
    if (is_32bit())
        return (void *)0x10000000; // 4GB
    else
        return (void *)((u64)1 << va_bits);
}

namespace {

void flush_tlb()
{
    asm volatile("tlbi vmalle1is" ::: "memory");
}

void flush_page(void *page)
{
    auto operand = reinterpret_cast<u64>(page) >> 12;
    asm volatile("tlbi vaae1is, %0" :: "r"(operand) : "memory");
}

void flush_range(void *start, size_t size)
{
    size_t page_size = PAGE_SIZE;
    if (size/page_size > 32) {
        flush_tlb();
        return;
    }

    for (char *i = (char *)start; i < (char *)start + size; i += page_size) {
        flush_page(i);
    }
}

} // namespace

void kernel::paging::arch_tlb_finalize(bool)
{
    dsb<Barrier::ISHST>();
    isb();
}

bool kernel::paging::arch_tlb_dont_ipi()
{
    return true;
}

void kernel::paging::flush_tlb_kernel()
{
    flush_tlb();
}

void kernel::paging::invalidate_tlb_kernel(void *page)
{
    flush_page(page);
}

void kernel::paging::invalidate_tlb_kernel(void *start, size_t size)
{
    flush_range(start, size);
}

void AArch64_Page_Table::invalidate_tlb(void *page)
{
    flush_page(page);
}

void AArch64_Page_Table::invalidate_tlb(void *start, size_t size)
{
    flush_range(start, size);
}

void AArch64_Page_Table::tlb_flush_all()
{
    flush_tlb();
}

void AArch64_Page_Table::apply()
{
    assert(page_directory != PAGE_DIRECTORY_INITIALIZER);
    asm volatile("msr ttbr0_el1, %0" :: "r"(page_directory) : "memory");
    flush_tlb();
    dsb<Barrier::ISHST>();
    isb();
}