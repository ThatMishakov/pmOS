/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "palloc.hh"

#include "mem.hh"
#include "paging.hh"
#include "virtmem.hh"

#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>

void *palloc(size_t number)
{
    // Find the suitable memory region
    void *ptr = kernel_space_allocator.virtmem_alloc(number);
    if (ptr == nullptr)
        return nullptr;

    // Allocate and try to map memory
    size_t i = 0;
    for (; i < number; ++i) {
        void *page        = nullptr;
        bool alloced_page = false;
        try {
            void *page   = kernel_pframe_allocator.alloc_page();
            alloced_page = true;

            void *virt_addr                        = (void *)((u64)ptr + i * 4096);
            static const Page_Table_Argumments arg = {
                .readable           = true,
                .writeable          = true,
                .user_access        = false,
                .global             = false,
                .execution_disabled = true,
                .extra              = 0,
            };

            map_kernel_page((u64)page, virt_addr, arg);
        } catch (Kern_Exception &e) {
            if (alloced_page)
                kernel_pframe_allocator.free(page);

            // Unmap and free the allocated pages
            for (size_t j = 0; j < i; ++j) {
                void *virt_addr = (void *)((u64)ptr + j * 4096);
                unmap_kernel_page(virt_addr);
            }
            kernel_space_allocator.virtmem_free(ptr, number);
            throw e;
        }
    }

    return ptr;
}