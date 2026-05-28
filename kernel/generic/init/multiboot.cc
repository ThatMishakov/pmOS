#if defined(__x86_64__) || defined(__i386__)

#include <multiboot2.h>
#include <types.hh>
#include <paging/arch_paging.hh>
#include <kern_logger/kern_logger.hh>
#include "common.hh"
#include <memory/vmm.hh>
#include <memory/paging.hh>
#include <memory/mem_object.hh>
#include "kernel_pages.hh"


using namespace kernel;
using namespace kernel::pmm;
using namespace kernel::log;
using namespace kernel::paging;
using namespace kernel::x86::paging;

#ifdef __i386__
using namespace kernel::ia32::paging;
#elif defined(__x86_64__)
using namespace kernel::x86_64::paging;
#endif

extern void *_kernel_start;
extern u8 _kernel_end;

extern u32 multiboot_cpu_features;
extern u32 multiboot_kernel_phys_offset;

extern u32 multiboot_temp_area;
extern u32 multiboot_temp_area_size;
extern u32 multiboot_temp_area_allocated;

extern u32 multiboot_kernel_base;

constexpr u32 NX_MASK = 1 << 0;
constexpr u32 PAGING_5LVL_MASK = 1 << 1;

struct multiboot_info {
    u32 total_size;
    u32 reserved;
};

class Init_Temp_Mapper: public Temp_Mapper
{
    virtual void *kern_map(u64 phys_frame) override
    {
        assert(phys_frame % PAGE_SIZE == 0);
        assert(phys_frame < (1ULL << 32)); // Only 4GB is mapped by the trampoline
        return (void *)(phys_frame);
    }
    virtual void return_map(void *) override { /* noop */ }
} multiboot_temp_mapper;

static Temp_Mapper *temp_temp_mapper = nullptr;
#ifdef __i386__
namespace kernel::ia32::paging
{
static Temp_Mapper *get_temp_temp_mapper(void *virt_addr, u32 kernel_cr3);
}
#else
static x86_64::paging::x86_PAE_Temp_Mapper temp_temp_mapper_instance;
static Temp_Mapper *get_temp_temp_mapper(void *virt_addr, ulong kernel_cr3)
{
    temp_temp_mapper_instance = x86_64::paging::x86_PAE_Temp_Mapper(virt_addr, kernel_cr3);
    return &temp_temp_mapper_instance;
}
#endif

extern void hcf();

static multiboot_tag *find_tag(multiboot_info *info, u32 type)
{
    u32 total_size = info->total_size;
    char *ptr      = (char *)info + sizeof(multiboot_info);
    while (ptr < (char *)info + total_size) {
        auto tag = (multiboot_tag *)ptr;
        if (tag->type == type)
            return tag;

        ptr += ((tag->size + 7) & ~7); // Tags are aligned on 8 bytes
    }
    return nullptr;
}

// static void multiboot_add_mmap_entry(uint64_t base, uint64_t length, uint32_t type)
// {
//     MemoryRegionType region_type;
//     switch (type) {
//     case MULTIBOOT_MEMORY_AVAILABLE:
//         region_type = MemoryRegionType::Usable;
//         break;
//     case MULTIBOOT_MEMORY_RESERVED:
//         region_type = MemoryRegionType::Reserved;
//         break;
//     case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
//         region_type = MemoryRegionType::Reclaimable;
//         break;
//     case MULTIBOOT_MEMORY_NVS:
//         region_type = MemoryRegionType::NVS;
//         break;
//     case MULTIBOOT_MEMORY_BADRAM:
//         region_type = MemoryRegionType::BadMemory;
//         break;
//     default:
//         region_type = MemoryRegionType::Unknown;
//         break;
//     }

//     const phys_addr_t PAGE_MASK = PAGE_SIZE - 1;
    
//     // If usable, align up the base and down the size, otherwise align up the size and down the base
//     if (region_type == MemoryRegionType::Usable) {
//         auto offset = base & PAGE_MASK;
//         base += offset ? PAGE_SIZE - offset : 0;
//         length = length > offset ? length - offset : 0;
//         length &= ~PAGE_MASK;
//     } else {
//         auto offset = base & PAGE_MASK;
//         base -= offset;
//         length += offset;
//         length += PAGE_MASK;
//         length &= ~PAGE_MASK;
//     }
//     memory_region_add({
//         .start = base,
//         .size  = length,
//         .type  = region_type,
//     });
// }

// static void multiboot_reserve_region(uint32_t base, uint32_t length)
// {
//     const phys_addr_t PAGE_MASK = PAGE_SIZE - 1;
//     uint64_t llength = length;

//     auto offset = base & PAGE_MASK;
//     base -= offset;
//     llength += offset;
//     llength += PAGE_MASK;
//     llength &= ~PAGE_MASK;
//     memory_region_add({
//         .start = base,
//         .size  = llength,
//         .type  = MemoryRegionType::UsableReservedOnBoot,
//     });
// }

static void init_memory(multiboot_info *info)
{
    assert(info);
    u32 multiboot_size = info->total_size;
    assert(multiboot_size >= sizeof(multiboot_info));

    serial_logger.printf("Initializing memory\n");

    global_temp_mapper = &multiboot_temp_mapper;

    temp_alloc_base = multiboot_temp_area + multiboot_temp_area_allocated;
    temp_alloc_size = multiboot_temp_area_size - multiboot_temp_area_allocated;

    #ifdef __i386__
    if (use_pae) {
        idle_cr3 = new_pae_cr3();
        if (idle_cr3 == -1U)
            panic("Failed to allocate memory for idle_cr3\n");
    } else {
        idle_cr3 = (u32)alloc_pages_from_temp_pool(1);
        if (idle_cr3 == 0)
            panic("Failed to allocate memory for idle_cr3\n");
        clear_page(idle_cr3, 0);
    }
    #elif defined(__x86_64__)
    idle_cr3 = (u64)alloc_pages_from_temp_pool(1);
    clear_page(idle_cr3, 0);
    #else
    #error "Unsupported architecture"
    #endif

    #ifdef __i386__
    u32 heap_space_start = 0xC0000000;
    #elif defined(__x86_64__)
    ulong heap_space_start = 0;
    if (kernel::x86_64::paging::use_5lvl_paging)
        heap_space_start = 0xFF00'0000'0000'0000;
    else
        heap_space_start = 0xFFFF'8000'0000'0000;
    #else
    #error "Unsupported architecture"
    #endif

    ulong heap_space_size  = 0 - heap_space_start;

    const ulong PAGE_MASK = PAGE_SIZE - 1;
    void *kernel_start     = (void *)(((ulong)&_kernel_start + PAGE_MASK) & ~PAGE_MASK);
    const ulong kernel_size =
        ((char *)&_kernel_end - (char *)kernel_start + PAGE_SIZE - 1) & ~PAGE_MASK;

    vmm::virtmem_init((void *)heap_space_start, heap_space_size, kernel_start, kernel_size);

    // 16 pages aligned to 16 pages boundary
    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate virtual memory for temp mapper\n");
    temp_temp_mapper = get_temp_temp_mapper(temp_mapper_start, idle_cr3);

    map_kernel_pages(idle_cr3, multiboot_kernel_base);

    global_temp_mapper = temp_temp_mapper;

    serial_logger.printf("Switching to kernel page tables...\n");
    apply_page_table(idle_cr3);

    serial_logger.printf("Paging initialized!\n");

    hcf();

    // // Init memory map...
    // klib::vector<char> multiboot_data;
    // if (!multiboot_data.resize(multiboot_size))
    //     panic("Failed to reserve memory for multiboot data copy\n");

    // copy_from_phys((ulong)info, multiboot_data.data(), multiboot_size);

    // auto memory_map = (multiboot_tag_mmap *)find_tag((multiboot_info *)multiboot_data.data(), MULTIBOOT_TAG_TYPE_MMAP);
    // assert(memory_map);

    // auto mmap_size = memory_map->size;
    // auto mmap_entry_size = memory_map->entry_size;
    // auto ptr = (char *)memory_map->entries;
    // auto end = (char *)memory_map + mmap_size;
    // while (ptr < end) {
    //     auto entry = (multiboot_mmap_entry *)ptr;
    //     multiboot_add_mmap_entry(entry->addr, entry->len, entry->type);
    //     ptr += mmap_entry_size;
    // }

    // // Reserve modules, mb2 info, and the kernel...
    // multiboot_reserve_region((uint32_t)(ulong)info, multiboot_size);
    // multiboot_reserve_region(multiboot_kernel_base, kernel_size);

    // // Reserve the temp area
    // multiboot_reserve_region(multiboot_temp_area, multiboot_temp_area_allocated);

    // char *ptr2 = (char *)multiboot_data.data() + sizeof(multiboot_info);
    // char *end2 = (char *)multiboot_data.data() + multiboot_size;
    // while (ptr2 < end2) {
    //     auto tag = (multiboot_tag *)ptr2;
    //     if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
    //         auto mod = (multiboot_tag_module *)tag;
    //         multiboot_reserve_region(mod->mod_start, mod->mod_end - mod->mod_start);
    //     }
    //     ptr2 += ((tag->size + 7) & ~7);
    // }

    // init_pmm(idle_cr3);
}

void init();

extern "C" void multiboot_main(multiboot_info* info) {
    early_detect_cpu_features();

    log::serial_logger.printf("Hello from pmOS kernel (booted with multiboot2)!\n");
    log::serial_logger.printf("Kernel start: 0x%lh\n", &_kernel_start);

    support_nx = (multiboot_cpu_features & NX_MASK) != 0;
    #if defined(__x86_64__)
    kernel::x86_64::paging::use_5lvl_paging = (multiboot_cpu_features & PAGING_5LVL_MASK) != 0;
    #endif

    init_memory(info);

    init();

    serial_logger.printf("hcf\n");
    hcf();
}

#endif