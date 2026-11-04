#include "aarch64_paging.hh"
#include <types.hh>
#include <arm_asm.hh>

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

u64 phys_addr_to_pte(u64 phys_addr);
u64 pte_to_phys_addr(u64 pte);

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
        dsb_ishst();
        return 0;
    } else {
        if (!(pte & DESCR_PRESENT)) {
            auto new_table = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_table))
                return -ENOMEM;

            clear_page(new_table);

            pte = construct_table(new_table, arg);
            store_pte(page_table, idx, pte);
            dsb_ishst();
        }
        if ((pte & DESCR_TABLE) == 0) {
            // Huge page
            return -EEXIST;
        }

        auto next_table = pte_to_phys_addr(pte);
        return map_page_recursive(next_table, phys_addr, virt_addr, arg, level - 1);
    }
}

} // namespace

kresult_t kernel::paging::map_page(u64 page_table, u64 phys_addr, void *virt_addr, ::kernel::paging::Page_Table_Arguments arg)
{
    return map_page_recursive(page_table, phys_addr, virt_addr, arg, top_level);
}
kresult_t kernel::paging::map_pages(ptable_top_ptr_t page_table, phys_addr_t phys_addr, void *virt_addr, size_t size_bytes, kernel::paging::Page_Table_Arguments arg);

kresult_t kernel::paging::map_kernel_page(phys_addr_t phys_addr, void *virt_addr, kernel::paging::Page_Table_Arguments arg)
{
    return map_page(kernel_page_table, phys_addr, virt_addr, arg);
}

kresult_t kernel::paging::map_kernel_pages(phys_addr_t phys_addr, void *virt_addr, size_t size, kernel::paging::Page_Table_Arguments arg)
{
    return map_pages(kernel_page_table, phys_addr, virt_addr, size, arg);
}