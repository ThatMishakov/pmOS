/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "riscv64_temp_mapper.hh"

#include "riscv64_paging.hh"

#include <memory/paging.hh>
#include <memory/temp_mapper.hh>

namespace kernel::riscv64::paging
{

RISCV64_Temp_Mapper::RISCV64_Temp_Mapper(void *virt_addr, u64 pt_ptr)
{
    pt_mapped = (RISCV64_PTE *)virt_addr;

    u64 addr    = (u64)virt_addr;
    start_index = addr / 4096 % 512;

    ::kernel::paging::Page_Table_Arguments arg {1, 1, 0, 0, 1, 000};

    auto leaf_pt_phys = prepare_leaf_pt_for(virt_addr, arg, pt_ptr);
    if (!leaf_pt_phys.success())
        panic("Failed to prepare leaf page table for temp mapper");

    ::kernel::paging::Temp_Mapper_Obj<RISCV64_PTE> tm(::kernel::paging::request_temp_mapper());
    RISCV64_PTE *pt = tm.map(leaf_pt_phys.val);

    RISCV64_PTE pte = RISCV64_PTE();
    pte.valid       = true;
    pte.readable    = true;
    pte.writeable   = true;
    pte.ppn         = leaf_pt_phys.val >> 12;
    pt[start_index] = pte;

    min_index = start_index + 1;
}

void *RISCV64_Temp_Mapper::kern_map(u64 phys_frame)
{
    for (unsigned i = min_index; i < start_index + size; ++i) {
        if (not pt_mapped[i].valid) {
            RISCV64_PTE pte = RISCV64_PTE();
            pte.valid       = true;
            pte.readable    = true;
            pte.writeable   = true;
            pte.ppn         = phys_frame >> 12;
            pt_mapped[i]    = pte;

            min_index = i + 1;
            char *t   = (char *)pt_mapped;
            t += (i - start_index) * 4096;

            // I've seen somewhere in RISC-V spec that the implementations
            // may cache the unmapped entries as well, so flush here wouldn't
            // hurt
            flush_page((void *)t);

            return (void *)t;
        }
    }

    return nullptr;
}

static u64 temp_mapper_get_index(u64 addr) { return (addr / 4096) % 512; }

void RISCV64_Temp_Mapper::return_map(void *p)
{
    if (p == nullptr)
        return;

    u64 i          = (u64)p;
    unsigned index = temp_mapper_get_index(i);

    pt_mapped[index] = RISCV64_PTE();
    if (index < min_index)
        min_index = index;

    flush_page(p);
}

} // namespace kernel::riscv64::paging