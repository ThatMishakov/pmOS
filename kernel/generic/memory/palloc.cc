#include "palloc.hh"
#include "virtmem.hh"
#include "mem.hh"
#include "paging.hh"
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>

void* palloc(size_t number)
{
    // Find the suitable memory region
    void * ptr = virtmem_alloc(number);
    if (ptr == nullptr)
        return nullptr;

    // Allocate and try to map memory
    size_t i = 0;
    for (; i < number; ++i) {
        void *page = nullptr;
        bool alloced_page = false;
        try {
            void * page = kernel_pframe_allocator.alloc_page();
            alloced_page = true;
            
            void * virt_addr = (void*)((u64)ptr + i*4096);
            static const Page_Table_Argumments arg = {
                .readable = true,
                .writeable = true,
                .user_access = false,
                .global = false,
                .execution_disabled = true,
                .extra = 0,
            };

            map_kernel_page((u64)page, virt_addr, arg);
        } catch (Kern_Exception &e) {
            if (alloced_page)
                kernel_pframe_allocator.free(page);
            
            // Unmap and free the allocated pages
            for (size_t j = 0; j < i; ++j) {
                void * virt_addr = (void*)((u64)ptr + j*4096);
                unmap_kernel_page(virt_addr);
            }
            virtmem_free(ptr, number);
            throw e;
        }
    }
    
    return ptr;
}