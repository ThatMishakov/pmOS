#include "palloc.hh"
#include "virtmem.hh"
#include "mem.hh"
#include "paging.hh"

void* palloc(size_t number)
{
    // Find the suitable memory region
    void * ptr = virtmem_alloc(number);
    if (ptr == nullptr)
        return nullptr;

    // Allocate and try to map memory
    size_t i = 0;
    for (; i < number; ++i) {
        void * page;
        try {
            page = kernel_pframe_allocator.alloc_page();
        } catch (...) {
            break;
        }
        

        void * virt_addr = (void*)((u64)ptr + i*4096);
        static const Page_Table_Argumments arg = {
            .readable = true,
            .writeable = true,
            .user_access = false,
            .global = false,
            .execution_disabled = true,
            .extra = 0,
        };
        kresult_t res = map_kernel_page((u64)page, virt_addr, arg);
        if (res != SUCCESS) {
            kernel_pframe_allocator.free(page);
            break;
        }
    }

    if (i != number) {
        // Unmap and free the allocated pages
        for (size_t j = 0; j < i; ++j) {
            void * virt_addr = (void*)((u64)ptr + j*4096);
            unmap_kernel_page(virt_addr);
        }
        virtmem_free(ptr, number);
        return nullptr;
    }
    
    return ptr;
}