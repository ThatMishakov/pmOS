#include <types.hh>
#include <memory/paging.hh>
#include "kernel_pages.hh"
#include <kern_logger/kern_logger.hh>
#include <memory/pmm.hh>
#include <memory/vmm.hh>

using namespace kernel;
using namespace kernel::paging;
using namespace kernel::log;
using namespace kernel::pmm;
using namespace kernel::vmm;

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
extern u8 _kernel_end;

void kernel::map_kernel_pages(ptable_top_ptr_t kernel_pt_top, phys_addr_t kernel_phys)
{
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

    // phys_addr_t for phys memory (because of PAE on i686), ulong for virtual memory/pointers

    const ulong kernel_start_virt = (ulong)&_kernel_start;
    const ulong kernel_text_start = kernel_start_virt & ~0xfff;
    const ulong kernel_text_end   = ((ulong)&_text_end + 0xfff) & ~0xfff;

    const phys_addr_t text_phys   = kernel_phys + kernel_text_start - kernel_start_virt;
    const ulong text_size   = kernel_text_end - kernel_text_start;
    const ulong text_virt   = text_phys - kernel_phys + kernel_start_virt;

    Page_Table_Arguments args = {
        .readable           = true,
        .writeable          = false,
        .user_access        = false,
        .global             = true,
        .execution_disabled = false,
        .extra              = PAGING_FLAG_STRUCT_PAGE,
    };

    kresult_t result = map_pages(kernel_pt_top, text_phys, (void *)text_virt, text_size, args);
    if (result)
        panic("Couldn't map kernel text\n");

    const ulong rodata_start  = (ulong)(&_rodata_start) & ~0xfff;
    const ulong rodata_end    = ((ulong)&_rodata_end + 0xfff) & ~0xfff;
    const ulong rodata_size   = rodata_end - rodata_start;
    const ulong rodata_offset = rodata_start - kernel_start_virt;
    const phys_addr_t rodata_phys     = kernel_phys + rodata_offset;
    const ulong rodata_virt   = kernel_start_virt + rodata_offset;
    args                    = {true, false, false, true, true, PAGING_FLAG_STRUCT_PAGE};
    result = map_pages(kernel_pt_top, rodata_phys, (void *)rodata_virt, rodata_size, args);
    if (result != 0)
        panic("Couldn't map kernel rodata\n");

    const ulong data_start  = (ulong)(&_data_start) & ~0xfffUL;
    const ulong data_end    = ((ulong)&_bss_end + 0xfffUL) & ~0xfffUL;
    const ulong data_size   = data_end - data_start;
    const ulong data_offset = data_start - kernel_start_virt;
    const phys_addr_t data_phys     = kernel_phys + data_offset;
    const ulong data_virt   = kernel_start_virt + data_offset;
    args                    = {true, true, false, true, true, PAGING_FLAG_STRUCT_PAGE};

    result                = map_pages(kernel_pt_top, data_phys, (void *)data_virt, data_size, args);
    if (result)
        panic("Couldn't map kernel data\n");

    const ulong eh_frame_start  = (ulong)(&__eh_frame_start) & ~0xfff;
    // Same as with data; merge eh_frame and gcc_except_table
    const ulong eh_frame_end    = ((ulong)&_gcc_except_table_end + 0xfff) & ~0xfff;
    const ulong eh_frame_size   = eh_frame_end - eh_frame_start;
    const ulong eh_frame_offset = eh_frame_start - kernel_start_virt;
    const phys_addr_t eh_frame_phys   = kernel_phys + eh_frame_offset;
    const ulong eh_frame_virt   = kernel_start_virt + eh_frame_offset;
    args                      = {true, false, false, true, true, PAGING_FLAG_STRUCT_PAGE};
    result = map_pages(kernel_pt_top, eh_frame_phys, (void *)eh_frame_virt, eh_frame_size, args);
    if (result)
        panic("Couldn't map kernel eh_frame\n");
}

#ifdef __i386__
phys_addr_t phys_memory_limit      = 0x100000000; // 4GB, if PAE is not enabled, otherwise up to 16GB
const phys_addr_t MAX_TOTAL_MEMORY = 0x400000000; // 16GB
extern bool use_pae;
#define ARCH_LIMITS_PHYS_MEMORY
#else
phys_addr_t phys_memory_limit      = 0;
#endif

phys_addr_t temp_alloc_base     = 0;
phys_addr_t temp_alloc_size     = 0;
phys_addr_t temp_alloc_reserved = 0;
long temp_alloc_entry_id = 0;

pmm::Page::page_addr_t alloc_pages_from_temp_pool(size_t pages)
{
    size_t size_bytes = pages * 4096;
    if (temp_alloc_reserved + size_bytes > temp_alloc_size)
        assert(!"Not enough memory in the temp pool");

    Page::page_addr_t addr = temp_alloc_base + temp_alloc_reserved;
    temp_alloc_reserved += size_bytes;
    return addr;
}

phys_addr_t total_memory = 0;

void kernel::init_pmm(ptable_top_ptr_t kernel_pt_top)
{
    #ifdef __i386__
    if (use_pae)
        phys_memory_limit = 0;
    #endif

    auto region_last_addr = [&](long index) -> phys_addr_t {
        return memory_map[index].start + memory_map[index].size;
    };

    auto new_region_func = [&](long first_index, long last_index) -> void {
        phys_addr_t first_addr = memory_map[first_index].start;
        phys_addr_t last_addr  = region_last_addr(last_index);
        phys_addr_t size       = last_addr - first_addr;

        #ifdef ARCH_LIMITS_PHYS_MEMORY
        if (phys_memory_limit != 0) {
            if (first_addr >= phys_memory_limit)
                return;

            if (last_addr >= phys_memory_limit) {
                last_addr = phys_memory_limit;
                size      = last_addr - first_addr;
            }
        } else if (total_memory + size > MAX_TOTAL_MEMORY) {
            size              = MAX_TOTAL_MEMORY - total_memory;
            last_addr         = first_addr + size;
            phys_memory_limit = last_addr;
            serial_logger.printf(
                "Warning: Total physical memory exceeds 16GB, limiting to 16GB\n");
        }
        #endif

        serial_logger.printf("Setting up PMM region %lh - %lh\n", first_addr, last_addr);

        if (size == 0)
            return;

        total_memory += size;

        // Calculate memory for the Page structs
        auto entries               = size / PAGE_SIZE + 2; // 2 reserved entries
        auto page_struct_page_size = (entries * sizeof(pmm::Page) + (PAGE_SIZE - 1)) & ~((phys_addr_t)(PAGE_SIZE - 1));

        // Try and allocate pages from pool...
        long alloc_index = -1;
        for (long i = first_index; i <= last_index; ++i) {
            auto region = memory_map[i];
            #ifdef ARCH_LIMITS_PHYS_MEMORY
            if (phys_memory_limit != 0 and region.start >= phys_memory_limit)
                break;
            #endif

            if (i == temp_alloc_entry_id)
                continue;

            #ifdef ARCH_LIMITS_PHYS_MEMORY        
            if (region.type == MemoryRegionType::Usable and (region.size >= page_struct_page_size) and
                (phys_memory_limit == 0 or
                 (region.start + page_struct_page_size <= phys_memory_limit))) {
                alloc_index = i;
                break;
            }
            #else
            if (region.type == MemoryRegionType::Usable and region.size >= page_struct_page_size) {
                alloc_index = i;
                break;
            }
            #endif
        }

        phys_addr_t phys_addr = 0;
        if (alloc_index != -1) {
            phys_addr = memory_map[alloc_index].start;
        } else {
            // Allocate memory from the temp pool
            phys_addr = alloc_pages_from_temp_pool(page_struct_page_size / PAGE_SIZE);
        }

        // Reserve virtual memory
        auto virt_addr =
            vmm::kernel_space_allocator.virtmem_alloc(page_struct_page_size / PAGE_SIZE);

        Page_Table_Arguments args = {
            .readable           = true,
            .writeable          = true,
            .user_access        = false,
            .global             = true,
            .execution_disabled = true,
            .extra              = PAGING_FLAG_STRUCT_PAGE,
        };
        auto result = map_pages(kernel_pt_top, phys_addr, virt_addr, page_struct_page_size, args);
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
            auto region = memory_map[i];
            #ifdef ARCH_LIMITS_PHYS_MEMORY
            if (phys_memory_limit != 0 and region.start >= phys_memory_limit)
                break;
            #endif

            if (region.type == MemoryRegionType::Usable) {
                if (i == temp_alloc_entry_id) {
                    // Mark as allocated and continue
                    Page *p   = pages + (memory_map[i].start - first_addr) / PAGE_SIZE;
                    Page *end = p + memory_map[i].size / PAGE_SIZE;

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

                #ifdef ARCH_LIMITS_PHYS_MEMORY
                phys_addr_t length = (region.start + region.size > phys_memory_limit) and
                                     (phys_memory_limit != 0)
                                 ? phys_memory_limit - region.start
                                 : region.size;
                #else
                phys_addr_t length = region.size;
                #endif

                if (i == alloc_index) {
                    assert(memory_map[i].size >= page_struct_page_size);

                    auto occupied_pages = page_struct_page_size / PAGE_SIZE;
                    auto base_page_index =
                        (memory_map[i].start - first_addr) / PAGE_SIZE;
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

                    // Mark end page as reserved so it doesn't get coalesced with uninitialized
                    // memory
                    pages[end_index].type = Page::PageType::Reserved;

                    Page *to_add_to_free                    = pages + free_index;
                    to_add_to_free->type                    = Page::PageType::PendingFree;
                    to_add_to_free->rcu_state.pages_to_free = end_index - free_index;
                    pmm::free_page(to_add_to_free);
                } else {
                    auto base_page_index =
                        (memory_map[i].start - first_addr) / PAGE_SIZE;
                    auto pages_count = length / PAGE_SIZE;

                    pages[base_page_index + pages_count].type = Page::PageType::Reserved;

                    pages[base_page_index].type                    = Page::PageType::PendingFree;
                    pages[base_page_index].rcu_state.pages_to_free = pages_count;
                    pmm::free_page(pages + base_page_index);
                }
            } else {
                auto base_page_index = (region.start - first_addr) / PAGE_SIZE;
                #ifdef ARCH_LIMITS_PHYS_MEMORY
                u64 last_addr    = (region.start + region.size > phys_memory_limit) and
                                        (phys_memory_limit != 0)
                                       ? phys_memory_limit
                                       : region.start + region.size;
                #else
                u64 last_addr    = region.start + region.size;
                #endif
                auto pages_count = (last_addr - region.start) / PAGE_SIZE;

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
        phys_addr_t current_addr = memory_map[first_index].start;
        u64 end_addr = memory_map[last_index].start + memory_map[last_index].size;
        auto current_index = first_index;

        while (current_addr < end_addr) {
            PMMRegion *region = PMMRegion::get(current_addr);
            assert(region);

            u64 final_addr = region->start + region->size_bytes;
            while (current_index != last_index and
                   memory_map[current_index + 1].start <= current_addr) {
                current_index++;
            }
            auto i = current_index;

            while (i <= last_index and
                   (final_addr == 0 or memory_map[i].start < final_addr)) {
                i++;
            }
            assert(i > current_index);
            auto last_index = i - 1;

            auto saved_first = memory_map[current_index];
            auto saved_last  = memory_map[last_index];

            if (current_addr > memory_map[current_index].start) {
                phys_addr_t diff = current_addr - memory_map[current_index].start;
                assert(diff < memory_map[current_index].size);
                memory_map[current_index].size -= diff;
                memory_map[current_index].start = current_addr;
            }
            if (final_addr != 0 and
                memory_map[last_index].start + memory_map[last_index].size >
                    final_addr) {
                phys_addr_t diff = memory_map[last_index].start + memory_map[last_index].size -
                                   final_addr;
                assert(diff > 0);
                memory_map[last_index].size -= diff;
            }

            phys_addr_t final_final_addr =
                memory_map[last_index].start + memory_map[last_index].size;
            assert(final_final_addr != 0);

            new_region_func(current_index, last_index);

            memory_map[current_index] = saved_first;
            memory_map[last_index]    = saved_last;

            current_addr  = final_final_addr;
            current_index = last_index;
        }
    };

    serial_logger.printf("Creating PMM regions\n");

    long first_index = -1;
    for (size_t i = 0; i < memory_map.size(); ++i) {
        auto type = memory_map[i].type;
        if (region_is_usable_ram(type)) {
            if (first_index == -1) {
                first_index = i;
            } else if (region_last_addr(i - 1) < memory_map[i].start) {
                split_into_regions(first_index, i - 1);
                first_index = i;
            }

            if (i == (memory_map.size() - 1)) {
                split_into_regions(first_index, i);
            }
        } else if (first_index != -1) {
            split_into_regions(first_index, i - 1);
            first_index = -1;
        }
    }

    // Add temp region
    Page *temp_region_page = pmm::find_page(temp_alloc_base);
    assert(temp_region_page);
    auto reserved_count = temp_alloc_reserved / PAGE_SIZE;
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

    #ifdef __i386__
    phys_addr_t below_1g_region_end =
        memory_map[temp_alloc_entry_id].start + memory_map[temp_alloc_entry_id].size;

    if (phys_memory_limit != 0 and below_1g_region_end > phys_memory_limit)
        below_1g_region_end = phys_memory_limit;

    assert(below_1g_region_end >= (temp_alloc_base + temp_alloc_size));
    phys_addr_t size          = below_1g_region_end - temp_alloc_base - temp_alloc_size;
    #else
    phys_addr_t size  = temp_alloc_size / PAGE_SIZE;
    #endif
    auto free_region  = temp_region_page + reserved_count;
    free_region->type = Page::PageType::PendingFree;
    free_region->rcu_state.pages_to_free = size - reserved_count;

    pmm::free_page(free_region);
    pmm::pmm_fully_initialized = true;

    serial_logger.printf("PMM initialized!\n");
}