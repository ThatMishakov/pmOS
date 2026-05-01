#include <uacpi/uacpi.h>
#include <uacpi/kernel_api.h>
#include <types.hh>
#include <kern_logger/kern_logger.hh>
#include <messaging/rights.hh>
#include "common.hh"

using namespace kernel::log;

phys_addr_t rsdp                       = -1ULL;

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

klib::vector<module> modules;

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
        auto &module     = modules[i];
        auto &descriptor = desc->modules[i];

        auto result = kernel::ipc::MemObjectRight::create_for_group(module.object, group);
        if (!result)
            panic("Failed to create right for the memory object during task 1 init!");

        serial_logger.printf("Module: %s, cmdline: %s\n", modules[i].path.c_str(), modules[i].cmdline.c_str());

        descriptor.memory_object_id = result.val->right_sender_id;
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