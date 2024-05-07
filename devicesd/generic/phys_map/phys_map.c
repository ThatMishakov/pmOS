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

#include <kernel/errors.h>
#include <kernel/flags.h>
#include <phys_map/phys_map.h>
#include <pmos/memory.h>
#include <pmos/special.h>
#include <pmos/system.h>
#include <stdint.h>

void *map_phys(void *phys_addr, size_t bytes)
{
    uint64_t addr_aligned = (uint64_t)phys_addr & ~(uint64_t)07777;
    uint64_t addr_offset  = (uint64_t)phys_addr - addr_aligned;
    uint64_t end_aligned  = (uint64_t)phys_addr + bytes;
    end_aligned = (end_aligned & ~(uint64_t)07777) + (end_aligned & (uint64_t)07777 ? 010000 : 0);
    size_t size = end_aligned - addr_aligned;

    mem_request_ret_t t =
        create_phys_map_region(0, NULL, size, PROT_READ | PROT_WRITE, addr_aligned);

    if (t.result != SUCCESS)
        return NULL;

    return (void *)((uint64_t)t.virt_addr + addr_offset);
}

void unmap_phys(void *virt_addr, size_t bytes)
{
    uint64_t addr_alligned = (uint64_t)virt_addr & ~(uint64_t)0777;
    uint64_t end_alligned  = (uint64_t)virt_addr + bytes;
    end_alligned = (end_alligned & ~(uint64_t)0777) + (end_alligned & (uint64_t)0777 ? 01000 : 0);
    size_t size_pages = (end_alligned - addr_alligned) >> 12;

    // TODO
}