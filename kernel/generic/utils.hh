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
#include <stddef.h>
#include "types.hh"

void int_to_string(i64 n, u8 base, char* str, int& length);

void uint_to_string(u64 n, u8 base, char* str, int& length);

extern "C" void t_print_bochs(const char *str, ...);

namespace klib { class string; }
void term_write(const klib::string&);

extern "C" size_t strlen(const char *str);

extern "C" int printf(const char *str,...);

extern "C" void *memcpy(void* to, const void* from, size_t size);
extern "C" void *memset(void *str, int c, size_t n);

bool prepare_user_buff_rd(const char* buff, size_t size);

bool prepare_user_buff_wr(char* buff, size_t size);

bool copy_from_user(char* to, const char* from, size_t size);

bool copy_to_user(const char* from, char* to, size_t size);

// Copies a frame (ppn)
void copy_frame(u64 from, u64 to);

void clear_page(u64 phys_addr, u64 pattern = 0);

void copy_from_phys(u64 phys_addr, void* to, size_t size);
klib::string capture_from_phys(u64 phys_addr);

template<class A>
const A& max(const A& a, const A& b) noexcept
{
    if (a > b) return a;
    return b;
}

template<class A>
const A& min(const A& a, const A& b) noexcept
{
    return a < b ? a : b;
}

class Logger;

void print_stack_trace(Logger& logger);