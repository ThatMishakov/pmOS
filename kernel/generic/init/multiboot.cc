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
#include <processes/tasks.hh>


using namespace kernel;
using namespace kernel::pmm;
using namespace kernel::log;
using namespace kernel::paging;
using namespace kernel::x86::paging;
using namespace kernel::proc;

#ifdef __i386__
using namespace kernel::ia32::paging;
#elif defined(__x86_64__)
using namespace kernel::x86_64::paging;
#endif

extern void *_kernel_start;
extern u8 _kernel_end;

extern u32 multiboot_cpu_features;
extern u32 multiboot_kernel_phys_offset;

extern u32 multiboot_temp_area_allocated;

extern u32 multiboot_kernel_base;

constexpr u32 NX_MASK = 1 << 0;
constexpr u32 PAGING_5LVL_MASK = 1 << 1;
constexpr u32 PAE_MASK = 1 << 2;

auto align_up_to_page(auto addr)
{
    typeof(addr) page_mask = PAGE_SIZE - 1;
    return (addr + page_mask) & ~page_mask;
}

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
Temp_Mapper *get_temp_temp_mapper(void *virt_addr, u32 kernel_cr3);
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

extern u8 multiboot_temp_area[];
static constexpr u32 TEMP_AREA_SIZE = 0x400000;
extern u32 multiboot_temp_area_allocated;

static MemoryRegion *temp_memory_regions = nullptr;
static size_t memory_regions_size = 0;
static size_t memory_regions_count = 0;

static void temp_regions_reserve(size_t count)
{
    if (count <= memory_regions_size)
        return;

    constexpr size_t regions_per_page = PAGE_SIZE / sizeof(MemoryRegion);
    static_assert(regions_per_page > 0);

    size_t missing = count - memory_regions_size;
    size_t pages = (missing + regions_per_page - 1) / regions_per_page;
    size_t bytes = pages * PAGE_SIZE;

    if (TEMP_AREA_SIZE - multiboot_temp_area_allocated < bytes)
        panic("Not enough temporary memory to build the mmap! Temp area size 0x%x, allocated 0x%x, wanted 0x%x\n", TEMP_AREA_SIZE, multiboot_temp_area_allocated, bytes);

    multiboot_temp_area_allocated += bytes;
    memory_regions_size += pages * regions_per_page;
}

static paging::MemoryRegionType type_from_multiboot(u32 type)
{
    switch (type) {
    case MULTIBOOT_MEMORY_AVAILABLE:
        return paging::MemoryRegionType::Usable;
    case MULTIBOOT_MEMORY_RESERVED:
        return paging::MemoryRegionType::Reserved;
    case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
        return paging::MemoryRegionType::ACPIReclaimable;
    case MULTIBOOT_MEMORY_NVS:
        return paging::MemoryRegionType::ACPINVS;
    case MULTIBOOT_MEMORY_BADRAM:
        return paging::MemoryRegionType::BadMemory;
    default:
        return paging::MemoryRegionType::Reserved;
    }
}

static bool more_restrictive(paging::MemoryRegionType a, paging::MemoryRegionType b)
{
    auto rank = [](paging::MemoryRegionType type) {
        switch (type) {
        case paging::MemoryRegionType::Usable:
            return 0;
        case paging::MemoryRegionType::UsableReservedOnBoot:
            return 1;
        case paging::MemoryRegionType::ACPIReclaimable:
            return 2;
        default:
        case paging::MemoryRegionType::Reserved:
            return 3;
        case paging::MemoryRegionType::ACPINVS:
            return 4;
        case paging::MemoryRegionType::Framebuffer:
            return 5;
        case paging::MemoryRegionType::BadMemory:
            return 6;
        }
    };

    return rank(a) > rank(b);
}

static auto insert_region = [](MemoryRegion *it, MemoryRegion region) {
    temp_regions_reserve(memory_regions_count + 1);
    auto old_end = temp_memory_regions + memory_regions_count;
    std::copy_backward(it, old_end, old_end + 1);
    *it = region;
    ++memory_regions_count;
};

static void build_memory_map(multiboot_info *info)
{
    serial_logger.printf("Filtering multiboot memory map...\n");

    auto mmap = (multiboot_tag_mmap *)find_tag(info, MULTIBOOT_TAG_TYPE_MMAP);
    if (!mmap)
        panic("No multiboot memory map!");

    if (mmap->size < sizeof(multiboot_tag_mmap))
        panic("Invalid multiboot memory map size!");
    if (mmap->entry_size < sizeof(multiboot_mmap_entry))
        panic("Invalid multiboot memory map entry size!");

    size_t entry_size = mmap->entry_size;
    size_t entries_count = (mmap->size - sizeof(*mmap))/entry_size;

    temp_memory_regions = (MemoryRegion *)(multiboot_temp_area + multiboot_temp_area_allocated);

    temp_regions_reserve(entries_count);

    for (size_t i = 0; i < entries_count; ++i) {
        auto r = (multiboot_mmap_entry *)((char *)(mmap + 1) + i*entry_size);

        // Align and push
        MemoryRegion region = {
            .start = r->addr,
            .size = r->len,
            .type = type_from_multiboot(r->type),
        };

        constexpr u64 phys_addr_mask = PAGE_SIZE - 1;

        if (region.type == paging::MemoryRegionType::Usable) {
            // Align usable up/inwards
            auto old_start = region.start;
            region.start += phys_addr_mask;
            region.start &= ~phys_addr_mask;
            auto diff = region.start - old_start;

            region.size = diff > region.size ? 0 : region.size - diff;
            region.size &= ~phys_addr_mask;
        } else {
            // Align everything else to the outer boundary
            auto offset = region.start & phys_addr_mask;
            region.start -= offset;
            if (region.size > 0) {
                region.size += offset;
                region.size += phys_addr_mask;
                region.size &= ~phys_addr_mask;
            }
        }

        if (region.size > 0)
            temp_memory_regions[memory_regions_count++] = region;
    }

    std::sort(temp_memory_regions, temp_memory_regions + memory_regions_count,
              [](const MemoryRegion &a, const MemoryRegion &b) { return a.start < b.start; });

    auto erase_region = [](size_t i) {
        std::copy(temp_memory_regions + i + 1, temp_memory_regions + memory_regions_count,
                   temp_memory_regions + i);
        --memory_regions_count;
    };

    for (size_t i = 0; i < memory_regions_count; ) {
        if (i + 1 >= memory_regions_count)
            break;
    
        auto &region = temp_memory_regions[i];
        auto &next = temp_memory_regions[i + 1];
        if (region.start + region.size < next.start) {
            ++i;
            continue;
        }

        if (region.start + region.size == next.start) {
            if (region.type == next.type) {
                temp_memory_regions[i].size += next.size;
                erase_region(i + 1);
            } else {
                ++i;
            }

            continue;
        }

        if (region.type == next.type) {
            auto end = std::max(region.start + region.size, next.start + next.size);
            temp_memory_regions[i].size = end - region.start;
            erase_region(i + 1);
        } else {
            // Oh no...
            if (region.start == next.start) {
                if (more_restrictive(region.type, next.type))
                    std::swap(region, next);
                
                if (region.size > next.size) {
                    auto new_region = region;
                    new_region.start = next.start + next.size;
                    new_region.size = region.start + region.size - new_region.start;

                    auto it = std::upper_bound(temp_memory_regions, temp_memory_regions + memory_regions_count,
                        new_region, [](const MemoryRegion &a, const MemoryRegion &b) {
                            return a.start < b.start;
                        });

                    
                    std::copy(temp_memory_regions + i + 1, it, temp_memory_regions + i);
                    *(it - 1) = new_region;
                } else {
                    erase_region(i);
                }
            } else {
                if (!more_restrictive(region.type, next.type)) {
                    if (region.start + region.size > next.start + next.size) {
                        auto new_region = region;
                        new_region.start = next.start + next.size;
                        new_region.size = region.start + region.size - new_region.start;

                        auto it = std::upper_bound(temp_memory_regions, temp_memory_regions + memory_regions_count,
                            new_region, [](const MemoryRegion &a, const MemoryRegion &b) {
                                return a.start < b.start;
                            });

                        insert_region(it, new_region);
                    }

                    region.size = next.start - region.start;
                } else {
                    if (next.start + next.size > region.start + region.size) {
                        auto copy = next;
                        copy.start = region.start + region.size;
                        copy.size = next.start + next.size - copy.start;

                        auto it = std::upper_bound(temp_memory_regions, temp_memory_regions + memory_regions_count,
                            copy, [](const MemoryRegion &a, const MemoryRegion &b) {
                                return a.start < b.start;
                            });

                        std::copy(temp_memory_regions + i + 2, it, temp_memory_regions + i + 1);
                        *(it - 1) = copy;
                    } else {
                        erase_region(i + 1);
                    }
                }
            }
        }
    }
}

static void print_memory_map()
{
    serial_logger.printf("Memory map:\n");
    for (size_t i = 0; i < memory_regions_count; ++i) {
        auto &region = temp_memory_regions[i];
        serial_logger.printf("  %lx - %lx type %i\n", region.start, region.start + region.size, region.type);
    }
}

static void memory_map_reserve(u64 start, u64 length)
{
    u64 page_mask = PAGE_SIZE - 1;

    serial_logger.printf("Reserving %lx - %lx in memory map\n", start, start + length);

    auto start_offset = start & page_mask;
    start -= start_offset;
    length += start_offset;
    length = (length + page_mask) & ~page_mask;

    auto it = std::upper_bound(temp_memory_regions, temp_memory_regions + memory_regions_count, start,
        [](u64 addr, const MemoryRegion &region) {
            return addr < region.start;
        });

    if (it == temp_memory_regions)
        panic("Tried to reserve memory for the region that's not available (bootloader bug?)\n");

    --it;

    if (it->start > start || it->start + it->size < start + length)
        panic("Tried to reserve memory for the region that's not available (bootloader bug?)\n");

    if (it->type != paging::MemoryRegionType::Usable)
        panic("Tried to reserve memory for the region that's not usable (bootloader bug?)\n");

    if (it->start == start && it->size == length) {
        it->type = paging::MemoryRegionType::UsableReservedOnBoot;
    } else if (it->start == start) {
        it->start += length;
        it->size -= length;

        MemoryRegion new_region = {
            .start = start,
            .size = length,
            .type = paging::MemoryRegionType::UsableReservedOnBoot,
        };

        insert_region(it, new_region);
    } else if (it->start + it->size == start + length) {
        it->size -= length;

        MemoryRegion new_region = {
            .start = start,
            .size = length,
            .type = paging::MemoryRegionType::UsableReservedOnBoot,
        };

        insert_region(it + 1, new_region);
    } else {
        // Split into 3 regions
        auto old_end = it->start + it->size;

        MemoryRegion middle_region = {
            .start = start,
            .size = length,
            .type = paging::MemoryRegionType::UsableReservedOnBoot,
        };

        MemoryRegion end_region = {
            .start = start + length,
            .size = old_end - (start + length),
            .type = paging::MemoryRegionType::Usable,
        };

        it->size = start - it->start;

        insert_region(it + 1, middle_region);
        insert_region(it + 2, end_region);
    }
}

void print_memory_map(multiboot_info *info)
{
    auto mmap = (multiboot_tag_mmap *)find_tag(info, MULTIBOOT_TAG_TYPE_MMAP);
    if (!mmap)
        panic("No multiboot memory map!");

    u8 *ptr = (u8 *)(mmap + 1);
    u8 *end = (u8 *)mmap + mmap->size;

    if (mmap->size < sizeof(multiboot_tag_mmap))
        panic("Invalid multiboot memory map size!");
    if (mmap->entry_size < sizeof(multiboot_mmap_entry))
        panic("Invalid multiboot memory map entry size!");

    serial_logger.printf("Multiboot e820:\n");
    while (ptr < end) {
        auto e = (multiboot_mmap_entry *)ptr;
        ptr += mmap->entry_size;
        serial_logger.printf("%lx - %lx type %i\n", e->addr, e->addr + e->len, e->type);
    }
}

#ifdef __i386__
constexpr u64 MAX_INIT_PHYS_ADDR = 0xC0000000; // Only 3GB is mapped by the trampoline in 32-bit mode
#elif defined(__x86_64__)
constexpr u64 MAX_INIT_PHYS_ADDR = 0x100000000; // 4GB
#else
#error "Unsupported architecture"
#endif

static void init_memory(multiboot_info *info)
{
    assert(info);
    u32 multiboot_size = info->total_size;
    assert(multiboot_size >= sizeof(multiboot_info));

    serial_logger.printf("Initializing memory\n");

    global_temp_mapper = &multiboot_temp_mapper;

    print_memory_map(info);

    build_memory_map(info);

    auto kernel_size = (char *)&_kernel_end - (char *)&_kernel_start;
    memory_map_reserve(multiboot_kernel_base, kernel_size);

    char *ptr2 = (char *)info + sizeof(multiboot_info);
    char *end2 = (char *)info + multiboot_size;
    while (ptr2 < end2) {
        auto tag = (multiboot_tag *)ptr2;
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            auto mod = (multiboot_tag_module *)tag;
            memory_map_reserve(mod->mod_start, mod->mod_end - mod->mod_start);
        }
        ptr2 += ((tag->size + 7) & ~7);
    }

    print_memory_map();

    // Find the largest available region and put the temp area there
    for (size_t i = 0; i < memory_regions_count; ++i) {
        auto &region = temp_memory_regions[i];
        if (region.start >= MAX_INIT_PHYS_ADDR)
            // Only below 4GB is mapped by the trampoline
            break;

        if (region.type != paging::MemoryRegionType::Usable)
            continue;

        auto size_below_4gb = region.start + region.size > MAX_INIT_PHYS_ADDR ? MAX_INIT_PHYS_ADDR - region.start : region.size;
        if (size_below_4gb > temp_alloc_size) {
            temp_alloc_base = region.start;
            temp_alloc_size = size_below_4gb;
            temp_alloc_entry_id = i;
        }
    }

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

    if (!memory_map.reserve(memory_regions_count))
        panic("Failed to reserve memory for memory map");

    for (size_t i = 0; i < memory_regions_count; ++i)
        memory_map.push_back(temp_memory_regions[i]);

    init_pmm(idle_cr3);
}

void init_modules(multiboot_info *info)
{
    char *ptr = (char *)info + sizeof(multiboot_info);
    char *end = (char *)info + info->total_size;
    while (ptr < end) {
        auto tag = (multiboot_tag *)ptr;
        ptr += ((tag->size + 7) & ~7);

        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            auto mod = (multiboot_tag_module *)tag;
            
            size_t name_len = mod->size - sizeof(multiboot_tag_module);

            auto path = module_path(mod->cmdline, name_len);
            auto cmdline = module_cmdline(mod->cmdline, name_len);

            serial_logger.printf("Module at 0x%x size %x path %s cmdline %s\n", mod->mod_start, mod->mod_end - mod->mod_start, path.c_str(), cmdline.c_str());

            if (path == "") {
                serial_logger.printf("Warning: Empty path for module\n");
                continue;
            }

            if (cmdline == "") {
                serial_logger.printf("Warning: Empty cmdline for module\n");
            }

            module m = {
                .path = std::move(path),
                .cmdline = std::move(cmdline),
                .phys_addr = mod->mod_start,
                .size = mod->mod_end - mod->mod_start,
                .object = Mem_Object::create_from_phys(mod->mod_start, align_up_to_page(mod->mod_end - mod->mod_start), true),
            };

            if (!m.object)
                panic("Failed to create memory object for module");
            if (!modules.push_back(std::move(m)))
                panic("Failed to add module to modules vector");
        }
    }
}

klib::shared_ptr<paging::Mem_Object> rsdp_object;
void init_acpi(multiboot_info *info)
{
    auto tag = find_tag(info, MULTIBOOT_TAG_TYPE_ACPI_OLD);
    if (!tag)
        tag = find_tag(info, MULTIBOOT_TAG_TYPE_ACPI_NEW);
    if (!tag) {
        serial_logger.printf("No ACPI RSDP found in multiboot info\n");
        return;
    }

    size_t rsdp_size = tag->size - sizeof(multiboot_tag);
    auto size_pages = align_up_to_page(rsdp_size);

    assert(size_pages/ PAGE_SIZE == 1);

    rsdp_object = Mem_Object::create(12, size_pages);
    if (!rsdp_object)
        panic("Failed to create memory object for RSDP\n");

    auto r = rsdp_object->write_from_kernel(0, (void *)(tag + 1), rsdp_size);
    if (!r.success() || !r.val)
        panic("Failed to write RSDP to memory object\n");

    auto page = rsdp_object->atomic_request_page(0, false);
    if (!page.success() || !page.val)
        panic("Failed to request page for RSDP\n");

    init_acpi(page.val.get_phys_addr());
}

klib::vector<klib::unique_ptr<load_tag_generic>>
    construct_load_tag_framebuffer(multiboot_info *info)
{
    klib::vector<klib::unique_ptr<load_tag_generic>> tags {};

    auto fb_tag = (multiboot_tag_framebuffer *)find_tag(info, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
    if (!fb_tag) {
        serial_logger.printf("No framebuffer found\n");
        return tags;
    }

    if (fb_tag->common.framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
        serial_logger.printf("Unsupported framebuffer type: %i\n", fb_tag->common.framebuffer_type);
        return tags;
    }

    klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_framebuffer;
    if (!tag)
        panic("Failed to allocate memory for framebuffer tag");

    tag->tag = LOAD_TAG_FRAMEBUFFER;
    tag->flags = 0;
    tag->offset_to_next = sizeof(load_tag_framebuffer);

    load_tag_framebuffer *desc = (load_tag_framebuffer *)tag.get();
    desc->framebuffer_addr     = fb_tag->common.framebuffer_addr;
    desc->framebuffer_pitch    = fb_tag->common.framebuffer_pitch;
    desc->framebuffer_width    = fb_tag->common.framebuffer_width;
    desc->framebuffer_height   = fb_tag->common.framebuffer_height;
    desc->framebuffer_bpp      = fb_tag->common.framebuffer_bpp;
    desc->memory_model         = fb_tag->common.framebuffer_type;
    desc->red_mask_size        = fb_tag->framebuffer_red_mask_size;
    desc->red_mask_shift       = fb_tag->framebuffer_red_field_position;
    desc->green_mask_size      = fb_tag->framebuffer_green_mask_size;
    desc->green_mask_shift     = fb_tag->framebuffer_green_field_position;
    desc->blue_mask_size       = fb_tag->framebuffer_blue_mask_size;
    desc->blue_mask_shift      = fb_tag->framebuffer_blue_field_position;
    desc->unused[0]            = 0;

    if (!tags.push_back(klib::move(tag)))
        panic("Failed to add framebuffer tag to tags vector");

    return tags;
}

void init_task1(multiboot_info* info)
{
    // Find task 1 module.
    // For now, just search for "bootstrap"
    module *task1                = nullptr;
    const klib::string bootstrap = "bootstrap";
    for (auto &m: modules) {
        serial_logger.printf("Module %s\n", m.cmdline.c_str());
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

    // Create new task and load ELF into it
    auto task = TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel::User);
    if (!task)
        panic("Failed to create task");
    task->name = "bootstrap";
    serial_logger.printf("Loading ELF...\n");

    auto group = proc::TaskGroup::create_for_task(task);
    if (!group)
        panic("Failed to create task group for task 1");

    auto object = ipc::MemObjectRight::create_for_group(task1->object, group.val, ipc::MemObjectRight::PERM_READ);
    if (!object)
        panic("Failed to create memory object right for task 1");

    // Pass the modules to the task
    klib::vector<klib::unique_ptr<load_tag_generic>> tags;
    tags = construct_load_tag_framebuffer(info);

    auto t = construct_load_tag_rsdp();
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add RSDP tag");

    t = construct_load_tag_for_modules(group.val);
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add modules tag");

    auto p = task->atomic_load_elf(object.val, task1->path, tags, group.val);
    if (!p.success() || !p.val)
        panic("Failed to load task 1: %i", p.result);
}

namespace kernel::paging
{
extern klib::shared_ptr<Arch_Page_Table> idle_page_table;
}

void init();

multiboot_info *copy_multiboot_to_temp(multiboot_info *info)
{
    assert(info->total_size > 0 && "Invalid multiboot info size");

    size_t page_aligned = align_up_to_page(info->total_size);
    if (page_aligned > TEMP_AREA_SIZE - multiboot_temp_area_allocated)
        panic("Not enough temporary memory to copy multiboot info. Temp area size 0x%x, allocated 0x%x, wanted 0x%x\n", TEMP_AREA_SIZE, multiboot_temp_area_allocated, page_aligned);
    auto ptr = (char *)multiboot_temp_area + multiboot_temp_area_allocated;
    multiboot_temp_area_allocated += page_aligned;

    memcpy(ptr, info, info->total_size);

    return (multiboot_info *)ptr;
}

extern "C" void multiboot_main(multiboot_info* info) {
    serial_logger.printf("Multiboot info: %x\n", info);

    early_detect_cpu_features();

    log::serial_logger.printf("Hello from pmOS kernel (booted with multiboot2)!\n");
    log::serial_logger.printf("Kernel start: 0x%lh\n", &_kernel_start);

    support_nx = (multiboot_cpu_features & NX_MASK) != 0;
    #if defined(__x86_64__)
    kernel::x86_64::paging::use_5lvl_paging = (multiboot_cpu_features & PAGING_5LVL_MASK) != 0;
    #elif defined(__i386__)
    use_pae = (multiboot_cpu_features & PAE_MASK) != 0;
    #endif

    auto temp_info = copy_multiboot_to_temp(info);

    init_memory(temp_info);

    init();

    init_acpi(temp_info);

    idle_page_table = Arch_Page_Table::create_empty();
    if (!idle_page_table)
        panic("Could not create idle page table");

    init_scheduling_on_bsp();

    // Switch to CPU-local temp mapper
    global_temp_mapper = nullptr;

    init_modules(temp_info);
    init_task1(temp_info);
    serial_logger.printf("Loaded kernel...\n");

    reclaim_kernel_init_memory();

    serial_logger.printf("Bootstrap CPU entering userspace\n");
}

#endif