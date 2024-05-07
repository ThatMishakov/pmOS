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

#pragma once
#include <types.hh>

// TODO: mem.hh is a stupid and undescriptive filename

/**
 * @brief Physical page frame allocator
 *
 * This structure holds a page frame allocator for the kernel. It is currently implemented as a
 * bitmap.
 *
 */
class PFrameAllocator
{
public:
    /// Frees a page fram
    void free(void *page);

    /// Frees a page frame identified by ppn (>>12)
    inline void free_ppn(u64 ppn) { return free((void *)(ppn << 12)); }

    /// Allocated a page
    void *alloc_page();

    /// Allocates a page frame
    u64 alloc_page_ppn();

    /// @brief Reserves a region of physical pages
    /// @param base Page start
    /// @param size Size of the region in bytes.
    void reserve(void *base, u64 size);

    /// @brief Inits a page fram allocator from the mapped bitmap and its size in number of elements
    /// @param bitmap Virtual address of the bitmap
    /// @param size Number of u64 elements in a bitmap
    void init(u64 *bitmap, u64 size);

    /// @brief Function to be called after initializing paging. Currently unused.
    void init_after_paging();

    /// @brief Returns the size of the bitmap in bytes
    u64 bitmap_size_pages() const;

private:
    Spinlock lock;
    // void mark(u64 base, u64 size, bool usable);

    /// @brief Reads a bit from the bitmap
    /// @param pos Bit position
    /// @param bitmap Bitmap vector virtual address
    /// @return Bit of the bitmap
    static bool bitmap_read_bit(u64 pos, u64 *bitmap);

    /// @brief Marks a bit in a bitmap
    /// @param pos Position
    /// @param b Value
    /// @param bitmap Bitmap vector virtual address
    static void bitmap_mark_bit(u64 pos, bool b, u64 *bitmap);

    /// @brief Marks the memory region to *useful*
    /// @param base Start of the physical region
    /// @param size Size of the region in bytes
    /// @param usable Should the region be marked as useful
    /// @param bitmap Bitmap vector virtual address
    static void mark(u64 base, u64 size, bool usable, u64 *bitmap);

    /// @brief Marks a single page
    /// @param base Physical address of the page
    /// @param usable Is the page should be marked as useful
    /// @param bitmap Virtual address of the bitmap
    static void mark_single(u64 base, bool usable, u64 *bitmap);

    /// @brief Virtual address of the bitmap vector
    u64 *bitmap = 0;

    /// @brief Number of entries in the bitmap vector
    u64 bitmap_size = 0;

    /// @brief Base from which to allocate pages that are gona be used in spaces which only support
    /// 16 bits.
    const u64 non_special_base = 0x100000; // Start from 1 MB

    /// @brief Smalles position of the bitmap to be allocated from
    u64 smallest = non_special_base >> (12 + 6);
};

/// Kernel page frame allocator
extern PFrameAllocator kernel_pframe_allocator;