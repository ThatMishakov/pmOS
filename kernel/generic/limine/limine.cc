/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "limine.h"

#include "../memory/paging.hh"
#include "../memory/pmm.hh"
#include "../memory/temp_mapper.hh"
#include "../memory/vmm.hh"

#include <dtb/dtb.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/mem_object.hh>
#include <paging/arch_paging.hh>
#include <processes/tasks.hh>

using namespace kernel;
using namespace kernel::pmm;

extern "C" void limine_main();
extern "C" void _limine_entry();

LIMINE_BASE_REVISION(1)

limine_bootloader_info_request boot_request = {
    .id       = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0,
    .response = nullptr,
};

limine_memmap_request memory_request = {
    .id       = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = nullptr,
};

limine_entry_point_request entry_point_request = {
    .id       = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .response = nullptr,
    .entry    = _limine_entry,
};

limine_kernel_address_request kernel_address_request = {
    .id       = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = nullptr,
};

limine_hhdm_request hhdm_request = {
    .id       = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = nullptr,
};

limine_paging_mode_request paging_request = {
    .id       = LIMINE_PAGING_MODE_REQUEST,
    .revision = 0,
    .response = nullptr,
#ifdef __riscv
    .mode = LIMINE_PAGING_MODE_RISCV_SV57,
#else
    .mode = LIMINE_PAGING_MODE_DEFAULT,
#endif
    .flags = 0,
};

// Halt and catch fire function.
static void hcf(void)
{
    // asm ("cli");
    // for (;;) {
    //     asm ("hlt");
    // }

    while (1)
        ;
}

Direct_Mapper init_mapper;

// Temporary temporary mapper
#ifdef __x86_64__
    #include <paging/x86_temp_mapper.hh>
using Arch_Temp_Mapper = x86_PAE_Temp_Mapper;
#elif defined(__riscv)
    #include <paging/riscv64_temp_mapper.hh>
using Arch_Temp_Mapper = RISCV64_Temp_Mapper;
#endif

Arch_Temp_Mapper temp_temp_mapper;

u64 hhdm_offset = 0;

size_t number_of_cpus = 1;

u64 temp_alloc_base     = 0;
u64 temp_alloc_size     = 0;
u64 temp_alloc_reserved = 0;
u64 temp_alloc_entry_id = 0;

pmm::Page::page_addr_t alloc_pages_from_temp_pool(size_t pages) noexcept
{
    size_t size_bytes = pages * 4096;
    if (temp_alloc_reserved + size_bytes > temp_alloc_size)
        assert(!"Not enough memory in the temp pool");

    Page::page_addr_t addr = temp_alloc_base + temp_alloc_reserved;
    temp_alloc_reserved += size_bytes;
    return addr;
}

extern void *_kernel_start;

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

ptable_top_ptr_t kernel_ptable_top = 0;

static klib::vector<limine_memmap_entry> *limine_memory_regions = nullptr;

void construct_paging()
{
    serial_logger.printf("Finding the largest memory region...\n");
    limine_memmap_response *r = memory_request.response;
    if (r == nullptr) {
        hcf();
    }
    limine_memmap_response resp = *r;

    hhdm_offset = hhdm_request.response->offset;

    for (uint64_t i = 0; i < resp.entry_count; i++) {
        uint64_t type = resp.entries[i]->type;
        if (type == LIMINE_MEMMAP_USABLE) {
            if (temp_alloc_size < resp.entries[i]->length) {
                temp_alloc_base     = resp.entries[i]->base;
                temp_alloc_size     = resp.entries[i]->length;
                temp_alloc_entry_id = i;
            }
        }

        // Print memory map
        serial_logger.printf(" Memory map region %i: 0x%h - 0x%h, type %i\n", i,
                             resp.entries[i]->base, resp.entries[i]->base + resp.entries[i]->length,
                             resp.entries[i]->type);
    }

    serial_logger.printf("Found the largest memory region at 0x%h, size 0x%h\n", temp_alloc_base,
                         temp_alloc_size);

    init_mapper.virt_offset = hhdm_request.response->offset;
    global_temp_mapper      = &init_mapper;

    serial_logger.printf("Initializing paging...\n");

#ifdef __riscv
    auto rr               = paging_request.response;
    riscv64_paging_levels = rr->mode + 3;
    serial_logger.printf("Using %i paging levels\n", riscv64_paging_levels);
#endif

    kresult_t result = 0;

    const u64 kernel_start_virt = (u64)&_kernel_start & ~0xfff;

// While we're here, initialize virtmem
// Give it the first page of the root paging level
#ifdef __riscv
    const u64 heap_space_shift = 12 + (riscv64_paging_levels - 1) * 9;
#else
    const u64 heap_space_shift = 12 + 27;
#endif

    const u64 heap_space_start = (-1UL) << (heap_space_shift + 8);
    const u64 heap_addr_size   = 1UL << heap_space_shift;
    vmm::virtmem_init(heap_space_start, heap_addr_size);

    kernel_ptable_top = pmm::get_memory_for_kernel(1);
    clear_page(kernel_ptable_top);

    // Init temp mapper with direct map, while it is still available
    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(
        16, 4); // 16 pages aligned to 16 pages boundary
    temp_temp_mapper = Arch_Temp_Mapper(temp_mapper_start, kernel_ptable_top);

    // Map kernel pages
    //
    // Prelude:
    // misha@Yoga:~/pmos/kernel$ readelf -l kernel

    // Elf file type is EXEC (Executable file)
    // Entry point 0xffffffff800200a8
    // There are 6 program headers, starting at offset 64

    // Program Headers:
    // Type           Offset             VirtAddr           PhysAddr
    //                 FileSiz            MemSiz              Flags  Align
    // RISCV_ATTRIBUT 0x000000000029d420 0x0000000000000000 0x0000000000000000
    //                 0x0000000000000048 0x0000000000000000  R      0x1
    // LOAD           0x0000000000001000 0xffffffff80000000 0xffffffff80000000
    //                 0x000000000002b728 0x000000000002b728  R E    0x1000
    // LOAD           0x000000000002c728 0xffffffff8002c728 0xffffffff8002c728
    //                 0x00000000000055f8 0x00000000000055f8  R      0x1000
    // LOAD           0x0000000000031d20 0xffffffff80032d20 0xffffffff80032d20
    //                 0x0000000000000878 0x0000000000007bf4  RW     0x1000
    // LOAD           0x0000000000032918 0xffffffff8003b918 0xffffffff8003b918
    //                 0x0000000000008c71 0x0000000000008c71  R      0x1000
    // DYNAMIC        0x0000000000000000 0x0000000000000000 0x0000000000000000
    //                 0x0000000000000000 0x0000000000000000  RW     0x8
    //...
    //  Section to Segment mapping:
    //    Segment Sections...
    //    00     .riscv.attributes
    //    01     .text
    //    02     .rodata .srodata .srodata._ZTS4Port ...
    //    03     .data .init_array ... .bss .sbss ...
    //    04     .eh_frame .gcc_except_table
    //
    // The kernel is loaded into higher half and has 4 memory regions:
    // 1. .text and related, with Read an Execute permissions
    // 2. .rodata, with Read permissions
    // 3. .data and .bss, with Read and Write permissions
    // 4. .eh_frame and .gcc_except_table, with Read only permissions
    // The addresses of these sections are known from the symbols, defined by linker script and
    // their physical location can be obtained from Kernel Address Feature Request by limine
    // protocol Map these pages and switch to kernel page table
    const u64 kernel_text_start = kernel_start_virt & ~0xfff;
    const u64 kernel_text_end   = ((u64)&_text_end + 0xfff) & ~0xfff;

    const u64 kernel_phys      = kernel_address_request.response->physical_base;
    const u64 text_phys        = kernel_phys + kernel_text_start - kernel_start_virt;
    const u64 text_size        = kernel_text_end - kernel_text_start;
    const u64 text_virt        = text_phys - kernel_phys + kernel_start_virt;
    Page_Table_Argumments args = {
        .readable           = true,
        .writeable          = false,
        .user_access        = false,
        .global             = true,
        .execution_disabled = false,
        .extra              = PAGING_FLAG_STRUCT_PAGE,
    };

    map_pages(kernel_ptable_top, text_phys, text_virt, text_size, args);

    const u64 rodata_start  = (u64)(&_rodata_start) & ~0xfff;
    const u64 rodata_end    = ((u64)&_rodata_end + 0xfff) & ~0xfff;
    const u64 rodata_size   = rodata_end - rodata_start;
    const u64 rodata_offset = rodata_start - kernel_start_virt;
    const u64 rodata_phys   = kernel_phys + rodata_offset;
    const u64 rodata_virt   = kernel_start_virt + rodata_offset;
    args                    = {true, false, false, true, true, PAGING_FLAG_STRUCT_PAGE};
    map_pages(kernel_ptable_top, rodata_phys, rodata_virt, rodata_size, args);
    if (result != 0)
        hcf();

    const u64 data_start  = (u64)(&_data_start) & ~0xfffUL;
    // Data and BSS are merged and have the same permissions
    const u64 data_end    = ((u64)&_bss_end + 0xfffUL) & ~0xfffUL;
    const u64 data_size   = data_end - data_start;
    const u64 data_offset = data_start - kernel_start_virt;
    const u64 data_phys   = kernel_phys + data_offset;
    const u64 data_virt   = kernel_start_virt + data_offset;
    args                  = {true, true, false, true, true, PAGING_FLAG_STRUCT_PAGE};
    map_pages(kernel_ptable_top, data_phys, data_virt, data_size, args);

    const u64 eh_frame_start  = (u64)(&__eh_frame_start) & ~0xfff;
    // Same as with data; merge eh_frame and gcc_except_table
    const u64 eh_frame_end    = ((u64)&_gcc_except_table_end + 0xfff) & ~0xfff;
    const u64 eh_frame_size   = eh_frame_end - eh_frame_start;
    const u64 eh_frame_offset = eh_frame_start - kernel_start_virt;
    const u64 eh_frame_phys   = kernel_phys + eh_frame_offset;
    const u64 eh_frame_virt   = kernel_start_virt + eh_frame_offset;
    args                      = {true, false, false, true, true, PAGING_FLAG_STRUCT_PAGE};
    map_pages(kernel_ptable_top, eh_frame_phys, eh_frame_virt, eh_frame_size, args);

    serial_logger.printf("Switching to in-kernel page table...\n");

    apply_page_table(kernel_ptable_top);

    // Set up the right mapper (since there is no direct map anymore) and bitmap
    global_temp_mapper = &temp_temp_mapper;

    // Allocate Page structs
    for (auto &i: kernel::pmm::free_pages_list)
        i.init();

    serial_logger.printf("Paging initialized!\n");

    klib::vector<limine_memmap_entry *> regions;
    if (!regions.resize(resp.entry_count))
        panic("Failed to reserve memory for regions");

    copy_from_phys((u64)resp.entries - hhdm_offset, regions.data(),
                   resp.entry_count * sizeof(limine_memmap_entry *));

    limine_memory_regions = new klib::vector<limine_memmap_entry> {};
    if (!limine_memory_regions)
        panic("Failed to allocate memory for memory_regions");

    klib::vector<limine_memmap_entry> &regions_data = *limine_memory_regions;
    if (!regions_data.resize(resp.entry_count))
        panic("Failed to reserve memory for regions_data");

    for (size_t i = 0; i < resp.entry_count; i++) {
        copy_from_phys((u64)regions[i] - hhdm_offset, &regions_data[i],
                       sizeof(limine_memmap_entry));
    }

    auto region_last_addr = [&](long index) -> u64 {
        return regions_data[index].base + regions_data[index].length;
    };

    auto new_region_func = [&](long first_index, long last_index) {
        auto base_addr = regions_data[first_index].base & ~0xfffUL;
        auto end_addr  = region_last_addr(last_index);

        serial_logger.printf("Setting up PMM region 0x%h - 0x%h\n", base_addr, end_addr);

        // Calculate memory for the Page structs
        auto range                 = (end_addr - base_addr + 0xfff) & ~0xfffUL;
        auto entries               = range / PAGE_SIZE + 2; // Reserved entries
        auto page_struct_page_size = (entries * sizeof(Page) + 0xfff) & ~0xfffUL;

        // Try and allocate memory from the pool itself
        long alloc_index = -1;
        for (auto i = first_index; i <= last_index; ++i) {
            if (i == (long)temp_alloc_entry_id)
                continue;

            if ((regions_data[i].type == LIMINE_MEMMAP_USABLE) and
                (regions_data[i].length >= page_struct_page_size)) {
                alloc_index = i;
                break;
            }
        }

        u64 phys_addr = 0;
        if (alloc_index != -1) {
            phys_addr = regions_data[alloc_index].base;
        } else {
            // Allocate memory from the temp pool
            phys_addr = alloc_pages_from_temp_pool(page_struct_page_size / PAGE_SIZE);
        }

        // Reserve virtual memory
        auto virt_addr =
            vmm::kernel_space_allocator.virtmem_alloc(page_struct_page_size / PAGE_SIZE);

        // Map the memory
        Page_Table_Argumments args = {
            .readable           = true,
            .writeable          = true,
            .user_access        = false,
            .global             = true,
            .execution_disabled = true,
            .extra              = PAGING_FLAG_STRUCT_PAGE,
        };
        map_pages(kernel_ptable_top, phys_addr, (u64)virt_addr, page_struct_page_size, args);

        Page *pages             = (Page *)virt_addr;
        // Reserve first and last page
        pages[0].type           = Page::PageType::Reserved;
        pages[entries - 1].type = Page::PageType::Reserved;

        // Map from the first page
        pages++;

        for (long i = first_index; i <= last_index; i++) {
            if (regions_data[i].type == LIMINE_MEMMAP_USABLE) {
                if ((size_t)i == temp_alloc_entry_id) {
                    // Mark as allocated and continue
                    Page *p   = pages + (regions_data[i].base - base_addr) / PAGE_SIZE;
                    Page *end = p + regions_data[i].length / PAGE_SIZE;

                    p->type       = Page::PageType::Allocated;
                    p->flags      = 0;
                    p->l.refcount = 1;

                    end[-1].type       = Page::PageType::Allocated;
                    end[-1].flags      = 0;
                    end[-1].l.refcount = 1;

                    continue;
                }

                if (i == alloc_index) {
                    assert(regions_data[i].length >= page_struct_page_size);
                    if (regions_data[i].length == page_struct_page_size)
                        continue;

                    auto occupied_pages  = page_struct_page_size / PAGE_SIZE;
                    auto base_page_index = (regions_data[i].base - base_addr) / PAGE_SIZE;
                    for (size_t j = base_page_index; j < base_page_index + occupied_pages; j++) {
                        pages[j].type       = Page::PageType::Allocated;
                        pages[j].flags      = 0;
                        pages[j].l.refcount = 1;
                        pages[j].l.next     = nullptr;
                    }

                    auto free_index = base_page_index + occupied_pages;
                    auto end_index =
                        free_index + (regions_data[i].length - page_struct_page_size) / PAGE_SIZE;

                    // Mark end page as reserved so it doesn't get coalesced with uninitialized
                    // memory
                    pages[end_index].type = Page::PageType::Reserved;

                    Page *to_add_to_free                    = pages + free_index;
                    to_add_to_free->type                    = Page::PageType::PendingFree;
                    to_add_to_free->rcu_state.pages_to_free = end_index - free_index;
                    pmm::free_page(to_add_to_free);
                } else {
                    auto base_page_index = (regions_data[i].base - base_addr) / PAGE_SIZE;
                    auto pages_count     = regions_data[i].length / PAGE_SIZE;

                    pages[base_page_index + pages_count].type = Page::PageType::Reserved;

                    pages[base_page_index].type                    = Page::PageType::PendingFree;
                    pages[base_page_index].rcu_state.pages_to_free = pages_count;
                    pmm::free_page(pages + base_page_index);
                }
            } else {
                auto base_page_index = (regions_data[i].base - base_addr) / PAGE_SIZE;
                auto pages_count     = regions_data[i].length / PAGE_SIZE;

                for (size_t j = base_page_index; j < base_page_index + pages_count; j++) {
                    pages[j].type       = Page::PageType::Allocated;
                    pages[j].flags      = 0;
                    pages[j].l.refcount = 1;
                    pages[j].l.next     = nullptr;
                }
            }
        }

        // Add the region to the PMM
        auto r = pmm::add_page_array(base_addr, entries - 2, pages);
        if (!r)
            assert(!"Failed to add region to PMM");
    };

    long first_index_of_range = -1;
    for (size_t i = 0; i < resp.entry_count; i++) {
        if (regions_data[i].type == LIMINE_MEMMAP_USABLE ||
            regions_data[i].type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
            regions_data[i].type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
            regions_data[i].type == LIMINE_MEMMAP_KERNEL_AND_MODULES) {
            if (first_index_of_range == -1) {
                first_index_of_range = i;
            } else if (region_last_addr(i - 1) < regions_data[i].base) {
                new_region_func(first_index_of_range, i - 1);
                first_index_of_range = i;
            }
            if (i == resp.entry_count - 1) {
                new_region_func(first_index_of_range, i);
            }
        } else if (first_index_of_range != -1) {
            new_region_func(first_index_of_range, i - 1);
            first_index_of_range = -1;
        }
    }

    // Add the temporary region
    Page *temp_region_page = pmm::find_page(temp_alloc_base);
    auto reserved_count    = temp_alloc_reserved / PAGE_SIZE;
    for (size_t i = 0; i < reserved_count; i++) {
        temp_region_page[i].type       = Page::PageType::Allocated;
        temp_region_page[i].flags      = 0;
        temp_region_page[i].l.refcount = 1;
    }

    auto size                            = temp_alloc_size / PAGE_SIZE;
    auto free_region                     = temp_region_page + reserved_count;
    free_region->type                    = Page::PageType::PendingFree;
    free_region->rcu_state.pages_to_free = size - reserved_count;

    pmm::free_page(free_region);
    pmm::pmm_fully_initialized = true;

    serial_logger.printf("PMM initialized!\n");
}

limine_module_request module_request = {
    .id                    = LIMINE_MODULE_REQUEST,
    .revision              = 1,
    .response              = nullptr,
    .internal_module_count = 0,
    .internal_modules      = nullptr,
};

struct module {
    klib::string path;
    klib::string cmdline;

    u64 phys_addr;
    u64 size;

    klib::shared_ptr<Mem_Object> object;
};

klib::vector<module> modules;

void init_modules()
{
    limine_module_response *resp = module_request.response;
    if (resp == nullptr)
        return;

    const u64 resp_phys = (u64)resp - hhdm_offset;
    limine_module_response r;
    copy_from_phys(resp_phys, &r, sizeof(r));

    klib::vector<limine_file *> modules;
    if (!modules.resize(r.module_count))
        panic("Failed to reserve memory for modules");

    const u64 files_phys = (u64)r.modules - hhdm_offset;
    copy_from_phys(files_phys, modules.data(), r.module_count * sizeof(limine_file *));

    if (!::modules.reserve(r.module_count))
        panic("Failed to reserve memory for modules");

    for (auto &mm: modules) {
        limine_file f;
        copy_from_phys((u64)mm - hhdm_offset, &f, sizeof(f));
        module m = {
            .path      = capture_from_phys((u64)f.path - hhdm_offset),
            .cmdline   = capture_from_phys((u64)f.cmdline - hhdm_offset),
            .phys_addr = (u64)f.address - hhdm_offset,
            .size      = f.size,
            .object    = Mem_Object::create_from_phys((u64)f.address - hhdm_offset,
                                                      (f.size + 0xfff) & ~0xfffUL, true),
        };

        serial_logger.printf("Module: %s, cmdline: %s\n", m.path.c_str(), m.cmdline.c_str());
        ::modules.push_back(m);
    }
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
    tag->tag                               = LOAD_TAG_LOAD_MODULES;
    tag->flags                             = 0;
    tag->offset_to_next                    = size;

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

struct limine_framebuffer_request fb_req = {
    .id       = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = nullptr,
};

klib::vector<klib::unique_ptr<load_tag_generic>> construct_load_tag_framebuffer()
{
    if (fb_req.response == nullptr)
        return {};

    struct limine_framebuffer_response r;
    copy_from_phys((u64)fb_req.response - hhdm_offset, &r, sizeof(r));

    limine_framebuffer *framebuffers[r.framebuffer_count];
    copy_from_phys((u64)r.framebuffers - hhdm_offset, framebuffers,
                   r.framebuffer_count * sizeof(limine_framebuffer *));

    klib::vector<klib::unique_ptr<load_tag_generic>> tags;
    if (!tags.reserve(r.framebuffer_count))
        panic("Failed to reserve memory for tags");

    for (size_t i = 0; i < r.framebuffer_count; i++) {
        limine_framebuffer fb;
        copy_from_phys((u64)framebuffers[i] - hhdm_offset, &fb, sizeof(fb));

        klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_framebuffer;
        tag->tag                               = LOAD_TAG_FRAMEBUFFER;
        tag->flags                             = 0;
        tag->offset_to_next                    = sizeof(load_tag_framebuffer);

        load_tag_framebuffer *desc = (load_tag_framebuffer *)tag.get();
        desc->framebuffer_addr     = (u64)fb.address - hhdm_offset;
        desc->framebuffer_pitch    = fb.pitch;
        desc->framebuffer_width    = fb.width;
        desc->framebuffer_height   = fb.height;
        desc->framebuffer_bpp      = fb.bpp;
        desc->memory_model         = fb.memory_model;
        desc->red_mask_size        = fb.red_mask_size;
        desc->red_mask_shift       = fb.red_mask_shift;
        desc->green_mask_size      = fb.green_mask_size;
        desc->green_mask_shift     = fb.green_mask_shift;
        desc->blue_mask_size       = fb.blue_mask_size;
        desc->blue_mask_shift      = fb.blue_mask_shift;
        desc->unused[0]            = 0;

        tags.push_back(klib::move(tag));
    }

    return tags;
}

constexpr u64 RSDP_INITIALIZER = (u64)-1;
u64 rsdp                       = RSDP_INITIALIZER;

klib::unique_ptr<load_tag_generic> construct_load_tag_rsdp()
{
    if (rsdp == RSDP_INITIALIZER)
        return {};

    klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_rsdp;

    auto *t   = (load_tag_rsdp *)tag.get();
    t->header = LOAD_TAG_RSDP_HEADER;
    t->rsdp   = rsdp;

    return tag;
}

klib::unique_ptr<load_tag_generic> construct_load_tag_fdt()
{
    if (not dtb_object)
        return nullptr;

    klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_fdt;

    auto t               = (load_tag_fdt *)tag.get();
    t->header            = LOAD_TAG_FDT_HEADER;
    t->fdt_memory_object = dtb_object->get_id();
    t->start_offset      = 0; // TODO?
    t->mem_object_size   = dtb_object->size_bytes();

    return tag;
}

klib::shared_ptr<Arch_Page_Table> idle_page_table = nullptr;

void init(void);
void init_scheduling(u64 boot_cpu_id);
klib::vector<u64> initialize_cpus(const klib::vector<u64> &hartids);
void *get_cpu_start_func();

void init_task1()
{
    // Find task 1 module.
    // For now, just search for "bootstrap"
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

    // Pass the modules to the task
    klib::vector<klib::unique_ptr<load_tag_generic>> tags;
    tags = construct_load_tag_framebuffer();

    auto t = construct_load_tag_rsdp();
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add RSDP tag");

    t = construct_load_tag_fdt();
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add FDT tag");

    tags.push_back(construct_load_tag_for_modules());

    // Create new task and load ELF into it
    auto task = TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel::User);
    serial_logger.printf("Loading ELF...\n");
    auto p = task->load_elf(task1->object, task1->path, tags);
    if (!p.success() || !p.val)
        panic("Failed to load task 1: %i", p.result);
}

#include <acpi/acpi.hh>
limine_rsdp_request rsdp_request = {
    .id       = LIMINE_RSDP_REQUEST,
    .revision = 0,
    .response = nullptr,
};

void init_acpi()
{
    if (rsdp_request.response == nullptr) {
        serial_logger.printf("No RSDP found\n");
        return;
    }

    limine_rsdp_response resp;
    copy_from_phys((u64)rsdp_request.response - hhdm_offset, &resp, sizeof(resp));

    if (resp.address == 0) {
        serial_logger.printf("RSDP not found\n");
        return;
    }

    u64 addr = (u64)resp.address - hhdm_offset;
    serial_logger.printf("RSDP found at 0x%x\n", addr);
    const bool b = enumerate_tables(addr);
    if (b)
        rsdp = addr;
}

limine_dtb_request dtb_request = {
    .id       = LIMINE_DTB_REQUEST,
    .revision = 0,
    .response = nullptr,
};

void init_dtb()
{
    if (dtb_request.response == nullptr) {
        return;
    }

    limine_dtb_response resp;
    copy_from_phys((u64)dtb_request.response - hhdm_offset, &resp, sizeof(resp));

    u64 addr = (u64)resp.dtb_ptr - hhdm_offset;
    serial_logger.printf("DTB found at 0x%x\n", addr);

    init_dtb((u64)addr);
}

struct limine_smp_request smp_request = {
    .id       = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = nullptr,
    .flags    = 0,
};

u64 bsp_cpu_id = 0;

void init_smp()
{
    if (smp_request.response == nullptr) {
        return;
    }

#ifdef __riscv
    limine_smp_response r;
    copy_from_phys((u64)smp_request.response - hhdm_offset, &r, sizeof(r));

    klib::vector<limine_smp_info *> smp_info;
    if (!smp_info.resize(r.cpu_count))
        panic("Failed to reserve memory for smp_info");
    
    copy_from_phys((u64)r.cpus - hhdm_offset, smp_info.data(),
                   r.cpu_count * sizeof(limine_smp_info *));

    klib::vector<u64> hartids;
    if (!hartids.reserve(r.cpu_count))
        panic("Failed to reserve memory for hartids");

    for (auto &info: smp_info) {
        limine_smp_info i;
        copy_from_phys((u64)info - hhdm_offset, &i, sizeof(i));
        if (i.hartid == bsp_cpu_id)
            continue;
        hartids.push_back(i.hartid);
    }

    auto v         = initialize_cpus(hartids);
    auto iter      = v.begin();
    auto jump_func = get_cpu_start_func();

    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());

    for (auto &info: smp_info) {
        limine_smp_info i;
        copy_from_phys((u64)info - hhdm_offset, &i, sizeof(i));
        if (i.hartid == bsp_cpu_id)
            continue;

        u64 offset = ((u64)info - hhdm_offset + offsetof(limine_smp_info, extra_argument)) & 0xfff;
        u64 addr = ((u64)info - hhdm_offset + offsetof(limine_smp_info, extra_argument)) & ~0xfffUL;
        mapper.map(addr);
        u64 *stack = (u64 *)((size_t)mapper.ptr + offset);
        *stack     = *iter;

        offset = ((u64)info - hhdm_offset + offsetof(limine_smp_info, goto_address)) & 0xfff;
        addr   = ((u64)info - hhdm_offset + offsetof(limine_smp_info, goto_address)) & ~0xfffUL;
        mapper.map(addr);
        void **func = (void **)((size_t)mapper.ptr + offset);
        *func       = jump_func;

        ++iter;
    }
#elif defined(__x86_64__)
    limine_smp_response r;
    copy_from_phys((u64)smp_request.response - hhdm_offset, &r, sizeof(r));

    klib::vector<limine_smp_info *> smp_info;
    if (!smp_info.resize(r.cpu_count))
        panic("Failed to reserve memory for smp_info");

    copy_from_phys((u64)r.cpus - hhdm_offset, smp_info.data(),
                   r.cpu_count * sizeof(limine_smp_info *));

    // Store as 64 bit ints to be inline with RISC-V
    klib::vector<u64> lapic_ids;
    for (auto &info: smp_info) {
        limine_smp_info i;
        copy_from_phys((u64)info - hhdm_offset, &i, sizeof(i));
        if (i.lapic_id == bsp_cpu_id)
            continue;

        lapic_ids.push_back(i.lapic_id);
    }

    auto v         = initialize_cpus(lapic_ids);
    auto iter      = v.begin();
    auto jump_func = get_cpu_start_func();
    for (auto &info: smp_info) {
        limine_smp_info i;
        copy_from_phys((u64)info - hhdm_offset, &i, sizeof(i));
        if (i.lapic_id == bsp_cpu_id)
            continue;

        u64 offset = ((u64)info - hhdm_offset + offsetof(limine_smp_info, extra_argument)) & 0xfff;
        u64 addr = ((u64)info - hhdm_offset + offsetof(limine_smp_info, extra_argument)) & ~0xfffUL;
        Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
        mapper.map(addr);
        u64 *stack = (u64 *)((size_t)mapper.ptr + offset);
        *stack     = *iter;

        offset = ((u64)info - hhdm_offset + offsetof(limine_smp_info, goto_address)) & 0xfff;
        addr   = ((u64)info - hhdm_offset + offsetof(limine_smp_info, goto_address)) & ~0xfffUL;
        mapper.map(addr);
        void **func = (void **)((size_t)mapper.ptr + offset);
        *func       = jump_func;

        ++iter;
    }
#elif
    #error "Unsupported architecture"
#endif
}

size_t booted_cpus = 0;
bool boot_barrier_start = false;

void limine_main()
{
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    serial_logger.printf("Hello from pmOS kernel!\n");
    serial_logger.printf("Kernel start: 0x%h\n", &_kernel_start);

    if (smp_request.response != nullptr) {
#ifdef __riscv
        number_of_cpus = smp_request.response->cpu_count;
        bsp_cpu_id     = smp_request.response->bsp_hartid;
#elif defined(__x86_64__)
        number_of_cpus = smp_request.response->cpu_count;
        bsp_cpu_id     = smp_request.response->bsp_lapic_id;
#endif
    }

    construct_paging();

    serial_logger.printf("Calling global constructors...\n");

    // Call global (C++) constructors
    init();

    init_acpi();
    init_dtb();

    // Init idle task page table
    idle_page_table = Arch_Page_Table::capture_initial(kernel_ptable_top);

    init_scheduling(bsp_cpu_id);

    // Switch to CPU-local temp mapper
    global_temp_mapper = nullptr;

    init_smp();

    init_modules();
    init_task1();
    serial_logger.printf("Loaded kernel...\n");

    __atomic_add_fetch(&booted_cpus, 1, __ATOMIC_RELAXED);

    while (__atomic_load_n(&booted_cpus, __ATOMIC_ACQUIRE) < number_of_cpus)
        ;
    // All CPUs are booted

    serial_logger.printf("All CPUs booted. Cleaning up...\n");

    // Release bootloader-reserved memory
    for (auto r: *limine_memory_regions) {
        if (r.type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            serial_logger.printf("Releasing bootloader reclaimable 0x%h - 0x%h\n", r.base, r.base + r.length);
            kernel::pmm::free_memory_for_kernel(r.base, r.length/PAGE_SIZE);
        }
    }

    delete limine_memory_regions;

    bool tmp = true;
    __atomic_store(&boot_barrier_start, &tmp, __ATOMIC_RELEASE);

    serial_logger.printf("Bootstrap CPU entering userspace\n");
}