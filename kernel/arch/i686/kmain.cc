#include "ultra_protocol.h"

#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <memory/pmm.hh>
#include <memory/temp_mapper.hh>
#include <memory/vmm.hh>
#include <types.hh>
#include <utils.hh>

using namespace kernel;

void hcf();

u32 temp_allocator_below_1gb   = 0;
u32 temp_allocated_bytes       = 0;
u32 temp_allocator_below_limit = 0;

pmm::Page::page_addr_t alloc_pages_from_temp_pool(size_t pages) noexcept
{
    size_t size_bytes = pages * 4096;
    if (temp_allocated_bytes + size_bytes > temp_allocator_below_limit)
        panic("Not enough memory in the temp pool\n");

    pmm::Page::page_addr_t addr = temp_allocator_below_1gb + temp_allocated_bytes;
    temp_allocated_bytes += size_bytes;
    return addr;
}

u32 idle_cr3 = 0;

class Init_Temp_Mapper: public Temp_Mapper
{
    virtual void *kern_map(u64 phys_frame) override
    {
        assert(phys_frame % 4096 == 0);
        assert(phys_frame < 0x40000000);
        return (void *)(phys_frame + 0xC0000000);
    }
    virtual void return_map(void *) override { /* noop */ }
} ultra_temp_mapper;
extern Temp_Mapper *global_temp_mapper;
Temp_Mapper *temp_temp_mapper = nullptr;
Temp_Mapper *get_temp_temp_mapper(void *virt_addr, u32 kernel_cr3);

extern void *_kernel_start;
extern void *_free_after_kernel;

extern void *_text_start;
extern void *_text_end;

extern void *_rodata_start;
extern void *_rodata_end;

extern void *_data_start;
extern void *_data_end;

extern void *_bss_start;
extern void *_bss_end;

extern void *__eh_frame_start;
extern void *__eh_frame_end;
extern void *_gcc_except_table_start;
extern void *_gcc_except_table_end;

ultra_attribute_header *find_attribute(ultra_boot_context *ctx, uint32_t type)
{
    ultra_attribute_header *hdr = ctx->attributes;
    uint32_t i                  = 0;
    while (hdr->type != type && i < ctx->attribute_count) {
        hdr = ULTRA_NEXT_ATTRIBUTE(hdr);
        i++;
    }

    if (i == ctx->attribute_count) {
        return nullptr;
    }

    return hdr;
}

void map_kernel(ultra_boot_context *ctx)
{
    auto kernel_info =
        (ultra_kernel_info_attribute *)find_attribute(ctx, ULTRA_ATTRIBUTE_KERNEL_INFO);

    const u32 kernel_start_virt = (u32)&_kernel_start;
    const u32 kernel_text_start = kernel_start_virt & ~0xfff;
    const u32 kernel_text_end   = ((u32)&_text_end + 0xfff) & ~0xfff;

    const u32 kernel_phys = (u32)kernel_info->physical_base;
    const u32 text_phys   = kernel_phys + kernel_text_start - kernel_start_virt;
    const u32 text_size   = kernel_text_end - kernel_text_start;
    const u32 text_virt   = text_phys - kernel_phys + kernel_start_virt;

    Page_Table_Argumments args = {
        .readable           = true,
        .writeable          = false,
        .user_access        = false,
        .global             = true,
        .execution_disabled = false,
        .extra              = PAGING_FLAG_STRUCT_PAGE,
    };

    kresult_t result = map_pages(idle_cr3, text_phys, text_virt, text_size, args);
    if (result)
        panic("Couldn't map kernel text\n");

    const u32 rodata_start  = (u32)(&_rodata_start) & ~0xfff;
    const u64 rodata_end    = ((u32)&_rodata_end + 0xfff) & ~0xfff;
    const u32 rodata_size   = rodata_end - rodata_start;
    const u32 rodata_offset = rodata_start - kernel_start_virt;
    const u32 rodata_phys   = kernel_phys + rodata_offset;
    const u32 rodata_virt   = kernel_start_virt + rodata_offset;
    args                    = {true, false, false, true, true, PAGING_FLAG_STRUCT_PAGE};
    result = map_pages(idle_cr3, rodata_phys, rodata_virt, rodata_size, args);
    if (result != 0)
        hcf();

    const u32 data_start  = (u64)(&_data_start) & ~0xfffUL;
    const u64 data_end    = ((u64)&_bss_end + 0xfffUL) & ~0xfffUL;
    const u64 data_size   = data_end - data_start;
    const u64 data_offset = data_start - kernel_start_virt;
    const u64 data_phys   = kernel_phys + data_offset;
    const u64 data_virt   = kernel_start_virt + data_offset;
    args                  = {true, true, false, true, true, PAGING_FLAG_STRUCT_PAGE};
    result                = map_pages(idle_cr3, data_phys, data_virt, data_size, args);
    if (result)
        panic("Couldn't map kernel data\n");

    const u64 eh_frame_start  = (u64)(&__eh_frame_start) & ~0xfff;
    // Same as with data; merge eh_frame and gcc_except_table
    const u64 eh_frame_end    = ((u64)&_gcc_except_table_end + 0xfff) & ~0xfff;
    const u64 eh_frame_size   = eh_frame_end - eh_frame_start;
    const u64 eh_frame_offset = eh_frame_start - kernel_start_virt;
    const u64 eh_frame_phys   = kernel_phys + eh_frame_offset;
    const u64 eh_frame_virt   = kernel_start_virt + eh_frame_offset;
    args                      = {true, false, false, true, true, PAGING_FLAG_STRUCT_PAGE};
    result = map_pages(idle_cr3, eh_frame_phys, eh_frame_virt, eh_frame_size, args);
    if (result)
        panic("Couldn't map kernel eh_frame\n");
}

void init_memory(ultra_boot_context *ctx)
{
    serial_logger.printf("Initializing memory\n");

    global_temp_mapper = &ultra_temp_mapper;

    auto mem = [=]() -> ultra_memory_map_attribute * {
        ultra_attribute_header *hdr = ctx->attributes;
        uint32_t i                  = 0;
        while (hdr->type != ULTRA_ATTRIBUTE_MEMORY_MAP && i < ctx->attribute_count) {
            hdr = ULTRA_NEXT_ATTRIBUTE(hdr);
            i++;
        }

        if (i == ctx->attribute_count) {
            panic("Memory map attribute not found\n");
        }

        return (ultra_memory_map_attribute *)hdr;
    }();

    auto number_of_entries = ULTRA_MEMORY_MAP_ENTRY_COUNT(mem->header);
    for (unsigned i = 0; i < number_of_entries; ++i) {
        serial_logger.printf(
            "Memory map entry %d: 0x%lx - 0x%lx, type %lx\n", i, mem->entries[i].physical_address,
            mem->entries[i].physical_address + mem->entries[i].size, mem->entries[i].type);
    }

    // Find the largest memory region below 1GB
    u32 largest_size = 0;
    for (unsigned i = 0; i < number_of_entries; ++i) {
        if (mem->entries[i].physical_address >= 0x40000000)
            break;

        auto end = mem->entries[i].physical_address + mem->entries[i].size;
        if (end > 0x40000000)
            end = 0x40000000;

        auto size = end - mem->entries[i].physical_address;
        if (size > largest_size) {
            largest_size               = size;
            temp_allocator_below_1gb   = mem->entries[i].physical_address;
            temp_allocator_below_limit = end;
        }
    }

    if (largest_size == 0) {
        panic("No memory region below 1GB\n");
    }
    serial_logger.printf("Allocating page tables from 0x%x - 0x%x...\n", temp_allocator_below_1gb,
                         temp_allocator_below_1gb + largest_size);

    idle_cr3 = (u32)alloc_pages_from_temp_pool(1);
    clear_page(idle_cr3, 0);

    u32 heap_space_start = (u32)&_free_after_kernel;
    u32 heap_space_size  = 0 - heap_space_start;
    vmm::virtmem_init(heap_space_start, heap_space_size);

    // 16 pages aligned to 16 pages boundary
    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate virtual memory for temp mapper\n");
    // Bruh
    temp_temp_mapper = get_temp_temp_mapper(temp_mapper_start, idle_cr3);

    map_kernel(ctx);

    global_temp_mapper = temp_temp_mapper;

    serial_logger.printf("Switching to kernel page tables...\n");
    apply_page_table(idle_cr3);

    // Allocate Page structs
    for (auto &i: kernel::pmm::free_pages_list)
        i.init();

    serial_logger.printf("Paging initialized!\n");
}

extern "C" void kmain(struct ultra_boot_context *ctx, uint32_t magic)
{
    if (magic != 0x554c5442) {
        panic("Not booted by Ultra\n");
    }

    serial_logger.printf("Booted by Ultra. ctx: %p\n", ctx);

    init_memory(ctx);

    hcf();
}