#include "ultra_protocol.h"

#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <memory/pmm.hh>
#include <memory/temp_mapper.hh>
#include <memory/vmm.hh>
#include <types.hh>
#include <utils.hh>

using namespace kernel;
using namespace kernel::pmm;

void hcf();

extern bool use_pae;

u32 temp_allocator_below_1gb   = 0;
u32 temp_allocated_bytes       = 0;
u32 temp_allocator_below_limit = 0;
long temp_alloc_entry_id       = 0;

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
    result                  = map_pages(idle_cr3, rodata_phys, rodata_virt, rodata_size, args);
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

u64 phys_memory_limit      = 0x100000000; // 4GB, if PAE is not enabled, otherwise up to 16GB
u64 total_memory           = 0;
const u64 MAX_TOTAL_MEMORY = 0x400000000; // 16GB

void pmm_create_regions(const klib::vector<ultra_memory_map_entry> &regions_data)
{
    if (use_pae)
        phys_memory_limit = 0;

    auto region_last_addr = [&](long index) -> u64 {
        return regions_data[index].physical_address + regions_data[index].size;
    };

    auto new_region_func = [&](long first_index, long last_index) -> void {
        serial_logger.printf("Setting up PMM region %lh - %lh\n",
                             regions_data[first_index].physical_address,
                             region_last_addr(last_index));
        u64 first_addr = regions_data[first_index].physical_address;
        u64 last_addr  = region_last_addr(last_index);
        u64 size       = last_addr - first_addr;

        if (!use_pae) {
            if (first_addr >= phys_memory_limit)
                return;

            if (last_addr >= phys_memory_limit) {
                last_addr = phys_memory_limit;
                size      = last_addr - first_addr;
            }
        } else {
            if (phys_memory_limit != 0)
                return;

            if (total_memory + size > MAX_TOTAL_MEMORY) {
                size              = MAX_TOTAL_MEMORY - total_memory;
                last_addr         = first_addr + size;
                phys_memory_limit = last_addr;
                serial_logger.printf(
                    "Warning: Total physical memory exceeds 16GB, limiting to 16GB\n");
            }
        }

        if (size == 0)
            return;

        total_memory += size;

        // Calculate memory for the Page structs
        auto entries               = size / PAGE_SIZE + 2; // 2 reserved entries
        auto page_struct_page_size = (entries * sizeof(pmm::Page) + 0xfff) & ~0xfffUL;

        // Try and allocate pages from pool...
        long alloc_index = -1;
        for (long i = first_index; i <= last_index; ++i) {
            auto region = regions_data[i];
            if (region.physical_address >= phys_memory_limit)
                break;

            if (i == temp_alloc_entry_id)
                continue;

            if (region.type == ULTRA_MEMORY_TYPE_FREE and (region.size >= page_struct_page_size) and
                (region.physical_address + page_struct_page_size <= phys_memory_limit)) {
                alloc_index = i;
                break;
            }
        }

        u64 phys_addr = 0;
        if (alloc_index != -1) {
            phys_addr = regions_data[alloc_index].physical_address;
        } else {
            // Allocate memory from the temp pool
            phys_addr = alloc_pages_from_temp_pool(page_struct_page_size / PAGE_SIZE);
        }

        // Reserve virtual memory
        auto virt_addr =
            vmm::kernel_space_allocator.virtmem_alloc(page_struct_page_size / PAGE_SIZE);

        Page_Table_Argumments args = {
            .readable           = true,
            .writeable          = true,
            .user_access        = false,
            .global             = true,
            .execution_disabled = true,
            .extra              = PAGING_FLAG_STRUCT_PAGE,
        };
        auto result = map_pages(idle_cr3, phys_addr, (u64)virt_addr, page_struct_page_size, args);
        if (result)
            panic("Couldn't map page structs\n");

        pmm::Page *pages        = (pmm::Page *)virt_addr;
        // Reserve first and last page
        pages[0].type           = pmm::Page::PageType::Reserved;
        pages[entries - 1].type = pmm::Page::PageType::Reserved;

        pages++;

        for (long i = first_index; i <= last_index; ++i) {
            auto region = regions_data[i];
            if (region.physical_address >= phys_memory_limit)
                break;

            if (region.type == ULTRA_MEMORY_TYPE_FREE) {
                if (i == temp_alloc_entry_id) {
                    // Mark as allocated and continue
                    Page *p   = pages + (regions_data[i].physical_address - first_addr) / PAGE_SIZE;
                    Page *end = p + regions_data[i].size / PAGE_SIZE;

                    p->type       = Page::PageType::Allocated;
                    p->flags      = 0;
                    p->l.refcount = 1;
                    p->l.owner    = nullptr; // Kernel memory

                    end[-1].type       = Page::PageType::Allocated;
                    end[-1].l.owner    = nullptr;
                    end[-1].flags      = 0;
                    end[-1].l.refcount = 1;

                    continue;
                }

                u64 length = region.physical_address + region.size > phys_memory_limit
                                 ? phys_memory_limit - region.physical_address
                                 : region.size;

                if (i == alloc_index) {
                    assert(regions_data[i].size >= page_struct_page_size);

                    auto occupied_pages = page_struct_page_size / PAGE_SIZE;
                    auto base_page_index =
                        (regions_data[i].physical_address - first_addr) / PAGE_SIZE;
                    for (size_t j = base_page_index; j < base_page_index + occupied_pages; j++) {
                        pages[j].type       = Page::PageType::Allocated;
                        pages[j].l.owner    = nullptr;
                        pages[j].flags      = 0;
                        pages[j].l.refcount = 1;
                        pages[j].l.next     = nullptr;
                    }

                    if (length == page_struct_page_size)
                        continue;

                    auto free_index = base_page_index + occupied_pages;
                    auto end_index  = free_index + (length - page_struct_page_size) / PAGE_SIZE;

                    pages[end_index].type = Page::PageType::Reserved;

                    Page *to_add_to_free                    = pages + free_index;
                    to_add_to_free->type                    = Page::PageType::PendingFree;
                    to_add_to_free->rcu_state.pages_to_free = end_index - free_index;
                    pmm::free_page(to_add_to_free);
                } else {
                    auto base_page_index =
                        (regions_data[i].physical_address - first_addr) / PAGE_SIZE;
                    auto pages_count = length / PAGE_SIZE;

                    pages[base_page_index + pages_count].type = Page::PageType::Reserved;

                    pages[base_page_index].type                    = Page::PageType::PendingFree;
                    pages[base_page_index].rcu_state.pages_to_free = pages_count;
                    pmm::free_page(pages + base_page_index);
                }
            } else {
                auto base_page_index = (region.physical_address - first_addr) / PAGE_SIZE;
                auto last_addr       = region.physical_address + region.size > phys_memory_limit
                                           ? phys_memory_limit
                                           : region.physical_address + region.size;
                auto pages_count     = (last_addr - region.physical_address) / PAGE_SIZE;

                for (size_t j = base_page_index; j < base_page_index + pages_count; j++) {
                    pages[j].type       = pmm::Page::PageType::Allocated;
                    pages[j].l.owner    = nullptr;
                    pages[j].flags      = 0;
                    pages[j].l.refcount = 1;
                    pages[j].l.next     = nullptr;
                }
            }
        }

        auto r = pmm::add_page_array(first_addr, entries - 2, pages);
        if (!r)
            panic("Failed to add region to PMM");
    };

    serial_logger.printf("Creating PMM regions\n");

    long first_index = -1;
    for (size_t i = 0; i < regions_data.size(); ++i) {
        auto type = regions_data[i].type;
        if (type == ULTRA_MEMORY_TYPE_FREE or type == ULTRA_MEMORY_TYPE_RECLAIMABLE or
            type == 0xFFFF0001 or type == ULTRA_MEMORY_TYPE_MODULE or
            type == ULTRA_MEMORY_TYPE_KERNEL_STACK or type == ULTRA_MEMORY_TYPE_KERNEL_BINARY) {
            if (first_index == -1) {
                first_index = i;
            } else if (region_last_addr(i - 1) < regions_data[i].physical_address) {
                new_region_func(first_index, i - 1);
                first_index = i;
            } else if (i == regions_data.size() - 1) {
                new_region_func(first_index, i);
            }
        } else if (first_index != -1) {
            new_region_func(first_index, i - 1);
            first_index = -1;
        }
    }

    // Add temp region
    Page *temp_region_page = pmm::find_page(temp_allocator_below_1gb);
    assert(temp_region_page);
    auto reserved_count = temp_allocated_bytes / PAGE_SIZE;
    for (size_t i = 0; i < reserved_count; i++) {
        temp_region_page[i].type       = Page::PageType::Allocated;
        temp_region_page[i].l.owner    = nullptr;
        temp_region_page[i].flags      = 0;
        temp_region_page[i].l.refcount = 1;
    }

    u64 below_1g_region_end =
        regions_data[temp_alloc_entry_id].physical_address + regions_data[temp_alloc_entry_id].size;
    if (below_1g_region_end > phys_memory_limit)
        below_1g_region_end = phys_memory_limit;

    assert(below_1g_region_end >= temp_allocator_below_1gb + temp_allocated_bytes);
    u64 size          = below_1g_region_end - temp_allocator_below_1gb - temp_allocated_bytes;
    auto free_region  = temp_region_page + reserved_count;
    free_region->type = Page::PageType::PendingFree;
    free_region->rcu_state.pages_to_free = size / PAGE_SIZE;

    pmm::free_page(free_region);
    pmm::pmm_fully_initialized = true;

    serial_logger.printf("PMM initialized!\n");
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
    auto entries           = mem->entries;
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
            temp_alloc_entry_id        = i;
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

    klib::vector<ultra_memory_map_entry> regions_data;
    if (!regions_data.resize(number_of_entries))
        panic("Failed to reserve memory for regions_data\n");

    copy_from_phys((u64)entries - 0xC0000000, regions_data.data(),
                   number_of_entries * sizeof(ultra_memory_map_entry));

    pmm_create_regions(regions_data);
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