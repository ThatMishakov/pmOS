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

#include "temp_mapper.hh"

#include "../sched/sched.hh"
#include "kernel/memory.h"
#include "paging.hh"

constexpr u64 temp_mapper_start_addr = 0177777'776'000'000'000'0000;
u64 temp_mapper_offset               = 0;

u64 temp_mapper_get_index(u64 addr) { return (addr / 4096) % 512; }

u64 temp_mapper_get_offset() { return temp_mapper_start_addr + temp_mapper_offset * 4096; }

void *Direct_Mapper::kern_map(u64 phys_frame) { return (void *)(virt_offset + phys_frame); }

void Direct_Mapper::return_map(void * /* unused */)
{
    // Do nothing and return
    return;
}

// nullptr indicates that the per-CPU mapper must be used
Temp_Mapper *global_temp_mapper = nullptr;
Temp_Mapper &request_temp_mapper()
{
    if (global_temp_mapper == nullptr) {
        // Use per-CPU mapper
        return get_cpu_struct()->get_temp_mapper();
    }

    // Use global mapper (typically during kernel initialization)
    return *global_temp_mapper;
}