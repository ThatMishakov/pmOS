#include "ultra_protocol.h"

#include <uacpi/uacpi.h>
#include <uacpi/kernel_api.h>
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
using namespace kernel::log;
using namespace kernel::paging;
using namespace kernel::ia32::paging;
using namespace kernel::x86::paging;

void hcf();

u32 temp_allocator_below_1gb   = 0;
u32 temp_allocated_bytes       = 0;
u32 temp_allocator_below_limit = 0;
extern long temp_alloc_entry_id;

pmm::Page::page_addr_t alloc_pages_from_temp_pool(size_t pages) noexcept
{
    size_t size_bytes = pages * 4096;
    if (temp_allocated_bytes + size_bytes > temp_allocator_below_limit)
        panic("Not enough memory in the temp pool\n");

    pmm::Page::page_addr_t addr = temp_allocator_below_1gb + temp_allocated_bytes;
    temp_allocated_bytes += size_bytes;
    return addr;
}

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
Temp_Mapper *temp_temp_mapper = nullptr;
namespace kernel::ia32::paging
{
Temp_Mapper *get_temp_temp_mapper(void *virt_addr, u32 kernel_cr3);
}

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

extern u8 _kernel_end;

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

    map_kernel_pages(kernel_ptable_top, kernel_info->physical_base);
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

        u64 end = mem->entries[i].physical_address + mem->entries[i].size;
        if (end > 0x40000000)
            end = 0x40000000;

        u64 size = end - mem->entries[i].physical_address;
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

    u32 heap_space_start = 0xC0000000;
    u32 heap_space_size  = 0 - heap_space_start;

    const size_t PAGE_MASK = PAGE_SIZE - 1;
    void *kernel_start     = (void *)(((size_t)&_kernel_start + PAGE_MASK) & ~PAGE_MASK);
    const size_t kernel_size =
        ((char *)&_kernel_end - (char *)kernel_start + PAGE_SIZE - 1) & ~PAGE_MASK;

    vmm::virtmem_init((void *)heap_space_start, heap_space_size, kernel_start, kernel_size);

    // 16 pages aligned to 16 pages boundary
    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate virtual memory for temp mapper\n");
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

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address)
{
    if (rsdp == RSDP_INITIALIZER)
        return UACPI_STATUS_NOT_FOUND;

    *out_rsdp_address = rsdp;
    return UACPI_STATUS_OK;
}

size_t uacpi_early_size = 0x1000;
char *uacpi_temp_buffer = nullptr;

void init();
void init_acpi(u64 rsdp_addr)
{
    if (rsdp_addr == 0) {
        serial_logger.printf("Warning: ACPI RSDP addr is 0...");
        return;
    }

    rsdp = rsdp_addr;

    for (;;) {
        uacpi_temp_buffer = new char[uacpi_early_size];
        if (!uacpi_temp_buffer)
            panic("Couldn't allocate memory for uACPI early buffer");
    
        auto ret = uacpi_setup_early_table_access((void *)uacpi_temp_buffer, uacpi_early_size);
        if (ret == UACPI_STATUS_OK)
        break;

        delete uacpi_temp_buffer;
        if (ret == UACPI_STATUS_OUT_OF_MEMORY) {
            uacpi_early_size += 4096;
        } else {
            serial_logger.printf("uacpi_initialize error: %s", uacpi_status_to_string(ret));
            return;
        }
    }
}

namespace kernel::paging
{
extern klib::shared_ptr<IA32_Page_Table> idle_page_table;
}

void init_scheduling_on_bsp();
void init_smp();

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

    // Align to u64
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

    auto task = proc::TaskDescriptor::create_process(proc::TaskDescriptor::PrivilegeLevel::User);
    if (!task)
        panic("Failed to create task");
    task->name = "bootstrap";
    serial_logger.printf("Loading ELF...\n");
    auto p = task->atomic_load_elf(task1->object, task1->path, tags);
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
    use_pae                            = attr.page_table_depth == 3;
    serial_logger.printf("PAE: %s (page depth %i)\n", use_pae ? "enabled" : "disabled",
                         attr.page_table_depth);

    auto nx_enabled = detect_nx();
    serial_logger.printf("NX: %s\n", nx_enabled ? "enabled" : "disabled");

    init_memory(ctx);

    // Call global (C++) constructors
    init();

    init_acpi(attr.acpi_rsdp_address);

    kernel::paging::idle_page_table = IA32_Page_Table::capture_initial(idle_cr3);

    init_scheduling_on_bsp();

    global_temp_mapper = nullptr;

    init_smp();

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