#include "io_bitmap.hh"
#include <sched/sched.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>

using namespace kernel;
using namespace kernel::sched;
using namespace kernel::paging;
using namespace kernel::pmm;

static bool have_tss_page = false;
static phys_addr_t tss_page_phys = 0;

static phys_addr_t get_tss_page_common()
{
    if (have_tss_page)
        return tss_page_phys;

    tss_page_phys = get_memory_for_kernel(1);
    if (alloc_failure(tss_page_phys))
        panic("Failed to allocate memory for TSS page!");

    have_tss_page = true;

    Temp_Mapper_Obj<u8> mapper(request_temp_mapper());
    mapper.map(tss_page_phys);

    memset(mapper.ptr, 0, PAGE_SIZE);
    *mapper.ptr = 0xff; // Fun x86 stuff (yes, this whole page is just for 1 byte)
    return tss_page_phys;
}

void setup_tss(CPU_Info *c)
{
    void *mapping = vmm::kernel_space_allocator.virtmem_alloc(4);
    if (!mapping)
        panic("Failed to alloc virt space for TSS!\n");

    auto phys_addr = get_memory_for_kernel(1);
    if (alloc_failure(phys_addr))
        panic("Failed to allocate memory for TSS!\n");

    static const Page_Table_Arguments arg = {
        .readable           = true,
        .writeable          = true,
        .user_access        = false,
        .global             = true,
        .execution_disabled = true,
    };

    auto result = map_kernel_page(phys_addr, mapping, arg);
    if (result)
        panic("Failed to map TSS page!\n");

    memset(mapping, 0, PAGE_SIZE);

    void *second_mapping = (char *)mapping + PAGE_SIZE * 3;

    // TODO: The arguments for the second (4th) mapping might be broken...

    result = map_kernel_page(get_tss_page_common(), second_mapping, arg);
    if (result)
        panic("Failed to map TSS page second time!\n");

    c->tss_virt = mapping;
}

namespace kernel::x86::gdt
{
    void io_bitmap_enable();
    void io_bitmap_disable();
}

namespace kernel::x86::cpus
{

static void io_bitmap_set(phys_addr_t bitmap_phys)
{
    auto c = get_cpu_struct();

    static const Page_Table_Arguments arg = {
        .readable = true,
        .writeable = false,
        .user_access = false,
        .global = false,
        .execution_disabled = true,
    };

    void *virt = (char *)c->tss_virt + PAGE_SIZE;

    auto result = kernel::paging::map_kernel_pages_overwrite(bitmap_phys, virt, PAGE_SIZE * 2, arg);
    // This can't fail
    assert(!result);
}

void apply_io_bitmap(phys_addr_t bitmap_phys, u64 page_table_id)
{
    auto c = get_cpu_struct();

    if (bitmap_phys == -1ULL && c->io_bitmap_active) {
        gdt::io_bitmap_disable();
        c->io_bitmap_active = false;
        return;
    }
    
    if (bitmap_phys != -1ULL) {
        if (c->io_bitmap_page_table != page_table_id) {
            io_bitmap_set(bitmap_phys);
            c->io_bitmap_page_table = page_table_id;
        }

        if (!c->io_bitmap_active) {
            gdt::io_bitmap_enable();
            c->io_bitmap_active = true;
        }
    }
}

}