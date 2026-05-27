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

static void init_memory(multiboot_info *info)
{
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
}

extern "C" void multiboot_main(multiboot_info* info) {
    early_detect_cpu_features();

    log::serial_logger.printf("Hello from pmOS kernel (booted with multiboot2)!\n");
    log::serial_logger.printf("Kernel start: 0x%lh\n", &_kernel_start);

    support_nx = (multiboot_cpu_features & NX_MASK) != 0;
    #if defined(__x86_64__)
    kernel::x86_64::paging::use_5lvl_paging = (multiboot_cpu_features & PAGING_5LVL_MASK) != 0;
    #endif

    init_memory(info);

    hcf();
}

#endif