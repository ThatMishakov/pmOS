#include <types.hh>
#include <kern_logger/kern_logger.hh>
#include <messaging/rights.hh>
#include "common.hh"
#include <flanterm.h>
#include <flanterm_backends/fb.h>
#include <memory/vmm.hh>
#include <memory/paging.hh>

using namespace kernel;
using namespace kernel::log;
using namespace kernel::paging;

#if defined(ENABLE_ACPI)
#include <uacpi/uacpi.h>
#include <uacpi/kernel_api.h>

phys_addr_t rsdp = -1ULL;
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address)
{
    if (rsdp == RSDP_INITIALIZER)
        return UACPI_STATUS_NOT_FOUND;

    *out_rsdp_address = rsdp;
    return UACPI_STATUS_OK;
}

size_t uacpi_early_size = 0x1000;
char *uacpi_temp_buffer = nullptr;

void init_acpi(phys_addr_t rsdp_addr)
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
#else
klib::unique_ptr<load_tag_generic> construct_load_tag_rsdp()
{
    return {};
}
#endif

pmos::containers::vector<boot_module> modules;

klib::unique_ptr<load_tag_generic> construct_load_tag_for_modules(kernel::proc::TaskGroup *group)
{
    assert(group);

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

    serial_logger.printf("Constructing load tag for %u modules\n", desc->modules_count);
    // Fill in the tags
    for (size_t i = 0; i < modules.size(); i++) {
        auto &mod         = modules[i];
        auto &descriptor = desc->modules[i];

        auto result = kernel::ipc::MemObjectRight::create_for_group(mod.object, group);
        if (!result)
            panic("Failed to create right for the memory object during task 1 init!");

        serial_logger.printf("Module: %s, cmdline: %s\n", modules[i].path.c_str(), modules[i].cmdline.c_str());

        descriptor.memory_object_id = result.val->right_sender_id;
        descriptor.size             = mod.size;
        memcpy((char *)tag.get() + string_offset, mod.path.c_str(), mod.path.size() + 1);
        descriptor.path_offset = string_offset;
        string_offset += mod.path.size() + 1;
        memcpy((char *)tag.get() + string_offset, mod.cmdline.c_str(),
               mod.cmdline.size() + 1);
        descriptor.cmdline_offset = string_offset;
        string_offset += mod.cmdline.size() + 1;
    }

    return tag;
}

char *strnchr(const char *s, size_t len, int c)
{
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == c)
            return (char *)(s + i);
    }

    return nullptr;
}

klib::string module_path(char *name, size_t max_len)
{
    auto separator = strnchr(name, max_len, ';');
    if (!separator)
        return klib::string(name, max_len);

    return klib::string(name, separator - name);
}
klib::string module_cmdline(char *name, size_t max_len)
{
    auto separator = strnchr(name, max_len, ';');
    if (!separator)
        return "";

    return klib::string(separator + 1, max_len - (separator - name) - 1);
}

extern "C" {
void *malloc(size_t size);
void free(void *ptr);
}

flanterm_context *ft_ctx = nullptr;
uint32_t *fb_virt        = nullptr;

// TODO: Enable this with cmdline
bool use_kernel_fb = false;

void init_kernel_fb(phys_addr_t fb_phys, unsigned width, unsigned height, unsigned pitch, unsigned bpp, unsigned red_mask, unsigned red_shift, unsigned green_mask, unsigned green_shift, unsigned blue_mask, unsigned blue_shift, unsigned memory_model)
{
    if (!use_kernel_fb)
        return;

    if (memory_model != 1) {
        serial_logger.printf("Unsupported framebuffer memory model: %u\n", memory_model);
        return;
    }

    const phys_addr_t page_mask = PAGE_SIZE - 1;

    size_t fb_size   = pitch * height;
    size_t fb_offset = size_t(fb_phys) & page_mask;
    fb_size          = (fb_size + fb_offset + page_mask) & ~page_mask;
    fb_virt          = (uint32_t *)vmm::kernel_space_allocator.virtmem_alloc(fb_size / PAGE_SIZE);
    if (!fb_virt)
        return;

    paging::Page_Table_Arguments args = {
        .readable           = true,
        .writeable          = true,
        .user_access        = false,
        .global             = true,
        .execution_disabled = true,
        .extra              = 0,
        .cache_policy       = paging::Memory_Type::Framebuffer,
    };

    uint64_t phys_addr_all = (uint64_t)fb_phys - fb_offset;
    auto result =
        map_kernel_pages(phys_addr_all, fb_virt, fb_size, args);
    if (result != 0) {
        serial_logger.printf("Failed to map framebuffer\n");
        vmm::kernel_space_allocator.virtmem_free((void *)fb_virt, fb_size / PAGE_SIZE);
        return;
    }

    fb_virt = (uint32_t *)((char *)fb_virt + fb_offset);

    ft_ctx = flanterm_fb_init(
        malloc, [](void *ptr, size_t) { free(ptr); }, fb_virt, width, height,
        pitch, red_mask, red_shift, green_mask, green_shift,
        blue_mask, blue_shift, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        0, 0, 1, 0, 0, 0, 0, true);
}
