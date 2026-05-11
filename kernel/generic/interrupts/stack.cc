#include "stack.hh"
#include <memory/vmm.hh>
#include <memory/pmm.hh>
#include <memory/paging.hh>

using namespace kernel;
using namespace kernel::vmm;
using namespace kernel::pmm;
using namespace kernel::paging;

Kernel_Stack_Pointer::Kernel_Stack_Pointer()
{
    auto vaddr = vmm::kernel_space_allocator.virtmem_alloc(total_size() / PAGE_SIZE);
    if (!vaddr)
        panic("Failed to allocate memory for kernel stack\n");

    auto paddr = pmm::get_memory_for_kernel(STACK_SIZE / PAGE_SIZE);
    if (pmm::alloc_failure(paddr))
        panic("Failed to allocate physical memory for kernel stack\n");

    void *actual_start = (void *)((char *)vaddr + GUARD_SIZE);

    Page_Table_Arguments arg = {
        .readable = true,
        .writeable = true,
        .user_access = false,
        .global = true,
        .execution_disabled = true,
    };

    auto result = map_kernel_pages(paddr, actual_start, STACK_SIZE, arg);
    if (result)
        panic("Failed to map kernel pages for stack, %i\n", result);
}