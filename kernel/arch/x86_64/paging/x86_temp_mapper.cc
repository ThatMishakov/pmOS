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

#include "x86_temp_mapper.hh"

#include "x86_paging.hh"

#include <types.hh>
#include <x86_asm.hh>

using namespace kernel::x86_64::paging;

x86_PAE_Temp_Mapper::x86_PAE_Temp_Mapper(void *virt_addr, u64 cr3)
{
    pt_mapped = (x86_PAE_Entry *)virt_addr;

    u64 addr    = (u64)virt_addr;
    start_index = addr / 4096 % 512;

    kernel::paging::Page_Table_Arguments arg {1, 1, 0, 0, 1, 000};

    auto pt_phys = prepare_pt_for(virt_addr, arg, cr3);
    if (pt_phys == -1UL)
        panic("Can't initialize temp mapper");

    kernel::paging::Temp_Mapper_Obj<x86_PAE_Entry> tm(kernel::paging::request_temp_mapper());
    x86_PAE_Entry *pt = tm.map(pt_phys);

    pt[start_index]           = x86_PAE_Entry();
    pt[start_index].present   = true;
    pt[start_index].writeable = true;
    pt[start_index].page_ppn  = pt_phys >> 12;

    min_index = start_index + 1;
}

void *x86_PAE_Temp_Mapper::kern_map(u64 phys_frame)
{
    for (unsigned i = min_index; i < start_index + size; ++i) {
        if (not pt_mapped[i].present) {
            pt_mapped[i].present   = true;
            pt_mapped[i].writeable = true;
            pt_mapped[i].page_ppn  = phys_frame >> 12;

            min_index = i + 1;

            char *t = (char *)pt_mapped;
            t += (i - start_index) * 4096;

            return (void *)t;
        }
    }

    assert(false);

    return nullptr;
}

static u64 temp_mapper_get_index(u64 addr) { return (addr / 4096) % 512; }

void x86_PAE_Temp_Mapper::return_map(void *p)
{
    if (p == nullptr)
        return;

    u64 i          = (u64)p;
    unsigned index = temp_mapper_get_index(i);

    pt_mapped[index] = x86_PAE_Entry();
    if (index < min_index)
        min_index = index;

    invlpg(i);
}