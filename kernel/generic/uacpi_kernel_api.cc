#include <uacpi/kernel_api.h>
#include <kern_logger/kern_logger.hh>
#include <pmos/utility/scope_guard.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>

using namespace kernel::log;
using namespace kernel::paging;
using namespace kernel::vmm;

void uacpi_kernel_log(uacpi_log_level ll, const uacpi_char* ss)
{
    serial_logger.printf("[Kernel uACPI %i] %s", ll, ss);
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    // Align start to page size
    const u64 mask = PAGE_SIZE - 1;
    u64 start = addr & ~mask;
    u64 end = (addr + len + mask) & ~mask;
    u64 size_aligned = end - start;
    u64 offset = addr % PAGE_SIZE;

    u64 size_pages = size_aligned / PAGE_SIZE;

    void *mapping = kernel_space_allocator.virtmem_alloc(size_pages);
    if (!mapping)
        return nullptr;
    auto guard = pmos::utility::make_scope_guard([=]() {
        kernel_space_allocator.virtmem_free(mapping, size_pages);
    });

    static const Page_Table_Arguments arg = {
        .readable = true,
        .writeable = true,
        .user_access = false,
        .global = false,
        .execution_disabled = true,
        .extra = PAGE_SPECIAL,
        // Cache policy normal
    };

    auto result = map_kernel_pages(start, mapping, size_aligned, arg);
    if (result)
        return nullptr;
    guard.dismiss();

    return (void *)((char *)mapping + offset);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    const uintptr_t mask = PAGE_SIZE - 1;
    uintptr_t start = (uintptr_t)addr & ~mask;
    uintptr_t end = ((uintptr_t)addr + len + mask) & ~mask;
    size_t size_aligned = end - start;

    size_t size_pages = size_aligned / PAGE_SIZE;

    {
        auto ctx = TLBShootdownContext::create_kernel();
        (void)unmap_kernel_pages(ctx, (void *)start, size_aligned);
    }

    kernel_space_allocator.virtmem_free((void *)start, size_pages);
}