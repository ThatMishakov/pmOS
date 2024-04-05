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

#ifndef KENREL_MEMORY_H
#define KENREL_MEMORY_H
#include "types.h"

typedef struct {
    u64 base_addr;
    u64 size;
} memory_descr;

typedef struct
{
    u8 present                   :1; 
    u8 writeable                 :1; 
    u8 user_access               :1;
    u8 write_through             :1;
    u8 cache_disable             :1;
    u8 accessed                  :1;
    u8 ignored                   :1;
    u8 reserved                  :1; // Must be 0
    u8 avl                       :3;  // Avaliable
    u8 pat                       :1;
    u64 page_ppn                   :40;
    u64 avl2                       :7;
    u8  pk                         :4;
    u8 execution_disabled       :1;
} PACKED PML4E;

typedef struct
{
    u8 present                   :1;
    u8 writeable                 :1;
    u8 user_access               :1;
    u8 write_through             :1;
    u8 cache_disabled            :1;
    u8 accessed                  :1;
    u8 ignored                   :1;
    u8 size                      :1;  // For 1 GB pages 
    u8 avl                       :3;  // Avaliable
    u8 ignored1                  :1;
    u64 page_ppn                 :40;
    u64 avl2                     :7;
    u8  pk                       :4;
    u8 execution_disabled        :1;
} PACKED PDPTE;

typedef struct
{
    u8 present                   :1;
    u8 writeable                 :1;
    u8 user_access               :1;
    u8 write_through             :1;
    u8 cache_disabled            :1;
    u8 accessed                  :1;
    u8 dirty                     :1;
    u8 size                      :1; 
    u8 avl                       :3;  // Avaliable
    u8 ignored                   :1;
    u64 page_ppn                 :40;
    u64 avl2                     :7;
    u8  pk                       :4;
    u8 execution_disabled       :1;
} PACKED PDE;

typedef struct
{
    u8 present                   :1;
    u8 writeable                 :1;
    u8 user_access               :1;
    u8 write_through             :1;
    u8 cache_disabled            :1;
    u8 accessed                  :1;
    u8 dirty                     :1;
    u8 pat                       :1;
    u8 global                    :1; 
    u8 avl                       :3;  // Avaliable
    u64 page_ppn                 :40;
    u64 avl2                     :7;
    u8  pk                       :4;
    u8 execution_disabled       :1;
} PACKED PTE;

#define PAGING_FLAG_NOFREE 0x02

typedef struct {
    PML4E entries[512];
} PACKED ALIGNED(0x1000) PML4;

typedef struct {
    PDPTE entries[512];
} PACKED ALIGNED(0x1000) PDPT;

typedef struct {
    PDE entries[512];
} PACKED ALIGNED(0x1000) PD;

typedef struct {
    PTE entries[512];
} PACKED ALIGNED(0x1000) PT;

#define PAGE_ADDR(page) (page.page_ppn << (12))

inline PML4* pml4_of(UNUSED u64 addr, u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12)) | (rec_map_index << (12+9)) | (rec_map_index << (12+9+9)) | (rec_map_index << (12+9+9+9));
    return ((PML4*)(01777770000000000000000 | offset));
}

inline PML4* pml4(u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12)) | (rec_map_index << (12+9)) | (rec_map_index << (12+9+9)) | (rec_map_index << (12+9+9+9));
    return ((PML4*)(01777770000000000000000 | offset));
}

inline PDPT* pdpt_of(u64 addr, u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12+9)) | (rec_map_index << (12+9+9)) | (rec_map_index << (12+9+9+9));
    addr = (u64)addr >> (9+9+9);
    addr &= 07770000;
    return ((PDPT*)((u64)01777770000000000000000 | addr | offset));
}

inline PD* pd_of(u64 addr, u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12+9+9)) | (rec_map_index << (12+9+9+9));
    addr = (u64)addr >> (9+9);
    addr &= 07777770000;
    return ((PD*)((u64)01777770000000000000000 | addr | offset));
}

inline PT* pt_of(u64 addr, u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12+9+9+9));
    addr = (u64)addr >> (9);
    addr &= 07777777770000;
    return ((PT*)((u64)01777770000000000000000 | addr | offset));
}

inline PML4E* get_pml4e(u64 addr, u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12)) | (rec_map_index << (12+9)) | (rec_map_index << (12+9+9)) | (rec_map_index << (12+9+9+9));
    addr = ((u64)addr >> (9+9+9+9)) & 07770;
    return ((PML4E*)((u64)01777770000000000000000 | addr | offset));
}

inline PDPTE* get_pdpe(u64 addr, u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12+9)) | (rec_map_index << (12+9+9)) | (rec_map_index << (12+9+9+9));
    addr = ((u64)addr >> (9+9+9)) & 07777770;
    return ((PDPTE*)((u64)01777770000000000000000 | addr | offset));
}

inline PDE* get_pde(u64 addr, u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12+9+9)) | (rec_map_index << (12+9+9+9));
    addr = ((u64)addr >> (9+9)) & 07777777770;
    return ((PDE*)((u64)01777770000000000000000 | addr | offset));
}

inline PTE* get_pte(u64 addr, u64 rec_map_index)
{
    const u64 offset = (rec_map_index << (12+9+9+9));
    addr = ((u64)addr >> (9)) & 07777777777770;
    return ((PTE*)((u64)01777770000000000000000 | addr | offset));
}

#endif