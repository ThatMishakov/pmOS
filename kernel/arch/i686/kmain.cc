#include <types.hh>
#include <memory/pmm.hh>
#include "ultra_protocol.h"
#include <utils.hh>
#include <kern_logger/kern_logger.hh>

using namespace kernel;

void hcf();

pmm::Page::page_addr_t alloc_pages_from_temp_pool(size_t pages) noexcept
{
    // TODO
    return 0;
}

extern "C" void _kmain_entry(struct ultra_boot_context *ctx, uint32_t magic)
{
    if (magic != 0x554c5442) {
        panic("Not booted by Ultra\n");
    }

    serial_logger.printf("Booted by Ultra. ctx: %p\n", ctx);

    hcf();
}