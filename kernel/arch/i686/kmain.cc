#include "ultra_protocol.h"

#include <acpi/acpi.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/mem_object.hh>
#include <memory/paging.hh>
#include <memory/pmm.hh>
#include <memory/temp_mapper.hh>
#include <memory/vmm.hh>
#include <paging/x86_paging.hh>
#include <processes/tasks.hh>
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

void pmm_create_regions(klib::vector<ultra_memory_map_entry> &regions_data)
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

        auto r = pmm::add_page_array(first_addr, entries - 2, pages);
        if (!r)
            panic("Failed to add region to PMM");

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
    };

    auto split_into_regions = [&](long first_index, long last_index) -> void {
        auto current_addr = regions_data[first_index].physical_address;
        auto end_addr = regions_data[last_index].physical_address + regions_data[last_index].size;
        auto current_index = first_index;

        while (current_addr < end_addr) {
            PMMRegion *region = PMMRegion::get(current_addr);
            assert(region);

            auto final_addr = region->start + region->size_bytes;
            while (current_index != last_index and regions_data[current_index + 1].physical_address <= current_addr) {
                current_index++;
            }
            auto i = current_index;

            while (i <= last_index and
                   (final_addr == 0 or regions_data[i].physical_address < final_addr)) {
                i++;
            }
            assert(i > current_index);
            auto last_index = i - 1;

            auto saved_first = regions_data[current_index];
            auto saved_last  = regions_data[last_index];

            if (current_addr > regions_data[current_index].physical_address) {
                auto diff = current_addr - regions_data[current_index].physical_address;
                assert(diff < regions_data[current_index].size);
                regions_data[current_index].size -= diff;
                regions_data[current_index].physical_address = current_addr;
            }
            if (final_addr != 0 and
                regions_data[last_index].physical_address + regions_data[last_index].size >
                    final_addr) {
                auto diff = regions_data[last_index].physical_address +
                            regions_data[last_index].size - final_addr;
                assert(diff > 0);
                regions_data[last_index].size -= diff;
            }

            auto final_final_addr =
                regions_data[last_index].physical_address + regions_data[last_index].size;
            assert(final_final_addr != 0);

            new_region_func(current_index, last_index);

            regions_data[current_index] = saved_first;
            regions_data[last_index]    = saved_last;

            current_addr  = final_final_addr;
            current_index = last_index;
        }
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
                split_into_regions(first_index, i - 1);
                first_index = i;
            } else if (i == regions_data.size() - 1) {
                split_into_regions(first_index, i);
            }
        } else if (first_index != -1) {
            split_into_regions(first_index, i - 1);
            first_index = -1;
        }
    }

    // Add temp region
    Page *temp_region_page = pmm::find_page(temp_allocator_below_1gb);
    assert(temp_region_page);
    auto reserved_count = temp_allocated_bytes / PAGE_SIZE;
    for (size_t i = 0; i < reserved_count;) {
        auto desc        = PageArrayDescriptor::find(temp_region_page + i);
        size_t first_idx = temp_region_page - desc->pages;
        size_t last_idx  = reserved_count - i + first_idx > desc->size ? desc->size - first_idx + i
                                                                       : reserved_count;
        for (; i < last_idx; ++i) {
            temp_region_page[i].type       = Page::PageType::Allocated;
            temp_region_page[i].l.owner    = nullptr;
            temp_region_page[i].flags      = 0;
            temp_region_page[i].l.refcount = 1;
        }
        if (i < reserved_count) {
            auto next = desc->next();
            assert(next);
            temp_region_page = next->pages;
        }
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

    serial_logger.printf("Paging initialized!\n");

    klib::vector<ultra_memory_map_entry> regions_data;
    if (!regions_data.resize(number_of_entries))
        panic("Failed to reserve memory for regions_data\n");

    copy_from_phys((u64)entries - 0xC0000000, regions_data.data(),
                   number_of_entries * sizeof(ultra_memory_map_entry));

    pmm_create_regions(regions_data);
}

constexpr u64 RSDP_INITIALIZER = -1ULL;
u64 rsdp                       = -1ULL;

void init();
void init_acpi(u64 rsdp_addr)
{
    if (rsdp_addr == 0) {
        serial_logger.printf("Warning: ACPI RSDP addr is 0...");
        return;
    }

    const bool b = enumerate_tables(rsdp_addr);
    if (b)
        rsdp = rsdp_addr;
}

extern klib::shared_ptr<IA32_Page_Table> idle_page_table;

void init_scheduling_on_bsp();

size_t ultra_context_size(struct ultra_boot_context *ctx)
{
    struct ultra_attribute_header *limit = ctx->attributes;
    for (unsigned i = 0; i < ctx->attribute_count; ++i)
        limit = ULTRA_NEXT_ATTRIBUTE(limit);

    return (char *)limit - (char *)ctx;
}

struct module {
    klib::string path;
    klib::string cmdline;

    u64 phys_addr;
    u64 size;

    klib::shared_ptr<Mem_Object> object;
};

klib::vector<module> modules;

char *strnchr(const char *s, size_t len, int c)
{
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == c)
            return (char *)(s + i);
    }

    return nullptr;
}

klib::string module_path(const ultra_module_info_attribute *t)
{
    auto separator = strnchr(t->name, 64, ';');
    if (!separator)
        return klib::string(t->name, 64);

    return klib::string(t->name, separator - t->name);
}
klib::string module_cmdline(const ultra_module_info_attribute *t)
{
    auto separator = strnchr(t->name, 64, ';');
    if (!separator)
        return "";

    return klib::string(separator + 1, 64 - (separator - t->name) - 1);
}

void init_modules(ultra_boot_context *ctx)
{
    size_t i                  = 0;
    ultra_attribute_header *h = ctx->attributes;
    auto next_module          = [&]() -> ultra_module_info_attribute          *{
        while (i < ctx->attribute_count) {
            auto t = h;
            h      = ULTRA_NEXT_ATTRIBUTE(h);
            ++i;

            if (t->type == ULTRA_ATTRIBUTE_MODULE_INFO)
                return (ultra_module_info_attribute *)t;
        }

        return nullptr;
    };

    u64 page_mask = PAGE_SIZE - 1;

    serial_logger.printf("Initializing modules...\n");

    ultra_module_info_attribute *t;
    while ((t = next_module())) {
        if (t->type == ULTRA_MODULE_TYPE_FILE) {
            serial_logger.printf("Ultra module at 0x%lx size %lx, name %s\n", t->address, t->size,
                                 t->name);

            auto path    = module_path(t);
            auto cmdline = module_cmdline(t);

            if (path == "") {
                serial_logger.printf("Warning: Empty path for module\n");
                continue;
            }

            if (cmdline == "") {
                serial_logger.printf("Warning: Empty cmdline for module\n");
            }

            module m = {
                .path      = klib::move(path),
                .cmdline   = klib::move(cmdline),
                .phys_addr = t->address,
                .size      = t->size,
                .object    = Mem_Object::create_from_phys(t->address,
                                                          (t->size + page_mask) & ~page_mask, true),
            };

            if (!m.object)
                panic("Failed to create memory object for module\n");

            if (!modules.push_back(m))
                panic("Failed to add module to modules\n");
        }
    }
}

klib::vector<klib::unique_ptr<load_tag_generic>>
    construct_load_tag_framebuffer(ultra_boot_context *ctx)
{
    klib::vector<klib::unique_ptr<load_tag_generic>> tags {};
    auto attrh =
        (ultra_framebuffer_attribute *)find_attribute(ctx, ULTRA_ATTRIBUTE_FRAMEBUFFER_INFO);
    if (!attrh) {
        serial_logger.printf("No framebuffer found\n");
        return tags;
    }
    auto attr = &attrh->fb;

    klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_framebuffer;
    if (!tag) {
        serial_logger.printf("Error: Couldn't allocate memory for framebuffer tag\n");
        return tags;
    }

    tag->tag                   = LOAD_TAG_FRAMEBUFFER;
    tag->flags                 = 0;
    tag->offset_to_next        = sizeof(load_tag_framebuffer);
    load_tag_framebuffer *desc = (load_tag_framebuffer *)tag.get();

    desc->framebuffer_addr   = attr->physical_address;
    desc->framebuffer_pitch  = attr->pitch;
    desc->framebuffer_width  = attr->width;
    desc->framebuffer_height = attr->height;
    desc->framebuffer_bpp    = attr->bpp;

    serial_logger.printf("Framebuffer: %dx%d, %d bpp, pitch %d at %lx\n", attr->width, attr->height,
                         attr->bpp, attr->pitch, attr->physical_address);

    switch (attr->format) {
    case ULTRA_FB_FORMAT_RGB888:
        desc->red_mask_size    = 8;
        desc->red_mask_shift   = 16;
        desc->green_mask_size  = 8;
        desc->green_mask_shift = 8;
        desc->blue_mask_size   = 8;
        desc->blue_mask_shift  = 0;
        break;
    case ULTRA_FB_FORMAT_BGR888:
        desc->red_mask_size    = 8;
        desc->red_mask_shift   = 0;
        desc->green_mask_size  = 8;
        desc->green_mask_shift = 8;
        desc->blue_mask_size   = 8;
        desc->blue_mask_shift  = 16;
        break;
    case ULTRA_FB_FORMAT_RGBX8888:
        desc->red_mask_size    = 8;
        desc->red_mask_shift   = 24;
        desc->green_mask_size  = 8;
        desc->green_mask_shift = 16;
        desc->blue_mask_size   = 8;
        desc->blue_mask_shift  = 8;
        break;
    case ULTRA_FB_FORMAT_XRGB8888:
        desc->red_mask_size    = 8;
        desc->red_mask_shift   = 16;
        desc->green_mask_size  = 8;
        desc->green_mask_shift = 8;
        desc->blue_mask_size   = 8;
        desc->blue_mask_shift  = 0;
        break;
    default:
        serial_logger.printf("Unknown framebuffer format: %d\n", attr->format);
        break;
    }

    desc->unused[0]    = 0;
    desc->memory_model = 1;

    if (!tags.push_back(klib::move(tag)))
        serial_logger.printf("Failed to add framebuffer tag\n");

    return tags;
}

klib::unique_ptr<load_tag_generic> construct_load_tag_rsdp(ultra_boot_context *)
{
    if (rsdp == RSDP_INITIALIZER)
        return {};

    klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_rsdp;
    if (!tag)
        return tag;

    auto *t   = (load_tag_rsdp *)tag.get();
    t->header = LOAD_TAG_RSDP_HEADER;
    t->rsdp   = rsdp;

    return tag;
}

klib::unique_ptr<load_tag_generic> construct_load_tag_for_modules()
{
    // Calculate the size
    u64 size = 0;

    // Header
    size += sizeof(load_tag_load_modules_descriptor);

    // module_descriptor tags
    size += sizeof(module_descriptor) * modules.size();
    u64 string_offset = size;

    // Strings
    for (const auto &t: modules) {
        size += t.path.size() + 1;
        size += t.cmdline.size() + 1;
    }

    // Allign to u64
    size = (size + 7) & ~7UL;

    // Allocate the tag
    // I think this is undefined behavior, but who cares :)
    klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new u64[size / 8];
    if (!tag) {
        panic("Failed to allocate memory for load tag");
        return nullptr;
    }

    tag->tag            = LOAD_TAG_LOAD_MODULES;
    tag->flags          = 0;
    tag->offset_to_next = size;

    load_tag_load_modules_descriptor *desc = (load_tag_load_modules_descriptor *)tag.get();

    desc->modules_count = modules.size();

    // Fill in the tags
    for (size_t i = 0; i < modules.size(); i++) {
        auto &module                = modules[i];
        auto &descriptor            = desc->modules[i];
        descriptor.memory_object_id = module.object->get_id();
        descriptor.size             = module.size;
        memcpy((char *)tag.get() + string_offset, module.path.c_str(), module.path.size() + 1);
        descriptor.path_offset = string_offset;
        string_offset += module.path.size() + 1;
        memcpy((char *)tag.get() + string_offset, module.cmdline.c_str(),
               module.cmdline.size() + 1);
        descriptor.cmdline_offset = string_offset;
        string_offset += module.cmdline.size() + 1;
    }

    return tag;
}

void init_task1(ultra_boot_context *ctx)
{
    module *task1                = nullptr;
    const klib::string bootstrap = "bootstrap";
    for (auto &m: modules) {
        if (m.cmdline == bootstrap) {
            task1 = &m;
            break;
        }
    }

    if (task1 == nullptr) {
        serial_logger.printf("Task 1 not found\n");
        hcf();
    }

    serial_logger.printf("Task 1 found: %s\n", task1->path.c_str());

    klib::vector<klib::unique_ptr<load_tag_generic>> tags;
    tags = construct_load_tag_framebuffer(ctx);

    auto t = construct_load_tag_rsdp(ctx);
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add RSDP tag");

    t = construct_load_tag_for_modules();
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add modules tag");

    auto task = TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel::User);
    if (!task)
        panic("Failed to create task");
    task->name = "bootstrap";
    serial_logger.printf("Loading ELF...\n");
    auto p = task->load_elf(task1->object, task1->path, tags);
    if (!p.success() || !p.val)
        panic("Failed to load task 1: %i", p.result);
}

extern "C" void kmain(struct ultra_boot_context *ctx, uint32_t magic)
{
    if (magic != 0x554c5442) {
        panic("Not booted by Ultra\n");
    }

    serial_logger.printf("Booted by Ultra. ctx: %p\n", ctx);

    auto size = ultra_context_size(ctx);
    serial_logger.printf("Ultra bootloade context size: %u\n", size);

    auto ptr = (ultra_platform_info_attribute *)find_attribute(ctx, ULTRA_ATTRIBUTE_PLATFORM_INFO);
    if (!ptr)
        panic("Could not find platform attribute!\n");
    ultra_platform_info_attribute attr = *ptr;

    init_memory(ctx);

    // Call global (C++) constructors
    init();

    init_acpi(attr.acpi_rsdp_address);

    idle_page_table = IA32_Page_Table::capture_initial(idle_cr3);

    init_scheduling_on_bsp();

    global_temp_mapper = nullptr;

    // Init SMP... TODO

    // Copy context to kernel...
    klib::unique_ptr<char> cctx = klib::unique_ptr<char>(new char[size]);
    if (!cctx)
        panic("Couldn't allocate memory for Ultra modules...");

    copy_from_phys((u32)ctx - 0xc0000000, (void *)cctx.get(), size);
    auto new_ctx = (ultra_boot_context *)cctx.get();

    init_modules(new_ctx);

    init_task1(new_ctx);

    serial_logger.printf("Entering userspace...\n");
}