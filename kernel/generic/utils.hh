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
#include "types.hh"

#include <stddef.h>
#include <optional>
#include <lib/vector.hh>

namespace kernel::log
{
struct Logger;
}

void int_to_string(i64 n, u8 base, char *str, int &length);

void uint_to_string(u64 n, u8 base, char *str, int &length);

extern "C" void t_print_bochs(const char *str, ...);

namespace klib
{
class string;
}
void term_write(const klib::string &);

extern "C" size_t strlen(const char *str);

extern "C" int printf(const char *str, ...);

extern "C" void *memcpy(void *to, const void *from, size_t size);
extern "C" void *memset(void *str, int c, size_t n);

ReturnStr<bool> prepare_user_buff_rd(const char *buff, size_t size);
ReturnStr<bool> prepare_user_buff_wr(char *buff, size_t size);

ReturnStr<bool> copy_from_user(char *to, const char *from, size_t size);
ReturnStr<bool> copy_to_user(const char *from, char *to, size_t size);

// Copies a frame (ppn)
void copy_frame(u64 from, u64 to);

void clear_page(u64 phys_addr, u64 pattern = 0);

void copy_from_phys(u64 phys_addr, void *to, size_t size);
klib::string capture_from_phys(u64 phys_addr);

ReturnStr<std::optional<klib::vector<char>>> to_buffer_from_user(void *ptr, size_t size);

template<class A> const A &max(const A &a, const A &b) noexcept
{
    if (a > b)
        return a;
    return b;
}

template<class A> const A &min(const A &a, const A &b) noexcept { return a < b ? a : b; }

void print_stack_trace(kernel::log::Logger &logger);

void panic(const char *msg, ...);

// Taken from
// https://github.com/managarm/managarm/blob/master/kernel/thor/generic/thor-internal/util.hpp#L14

inline int ceil_log2(uint64_t x) { return 64 - __builtin_clzll(x); }

struct U128 {
    uint64_t lo; // lower 64 bits
    uint64_t hi; // higher 64 bits
};

// Multiply two 64-bit numbers and return the 128-bit result in a U128.
static inline U128 mul64(uint64_t a, uint64_t b)
{
    const uint64_t mask = 0xFFFFFFFFULL;
    U128 r;
    // Split a and b into 32-bit halves.
    uint64_t a0 = a & mask;
    uint64_t a1 = a >> 32;
    uint64_t b0 = b & mask;
    uint64_t b1 = b >> 32;

    // Compute the four partial products.
    uint64_t z0 = a0 * b0;
    uint64_t z1 = a0 * b1;
    uint64_t z2 = a1 * b0;
    uint64_t z3 = a1 * b1;

    // Combine the results.
    // First, add the lower half of z0 and the lower halves of z1 and z2.
    uint64_t t = (z0 >> 32) + (z1 & mask) + (z2 & mask);
    r.lo       = (z0 & mask) | (t << 32);
    r.hi       = z3 + (z1 >> 32) + (z2 >> 32) + (t >> 32);
    return r;
}

// Shift a U128 right by 'shift' bits.
static inline U128 u128_shr(U128 x, unsigned int shift)
{
    U128 r;
    if (shift == 0) {
        return x;
    } else if (shift < 64) {
        r.lo = (x.lo >> shift) | (x.hi << (64 - shift));
        r.hi = x.hi >> shift;
    } else if (shift < 128) {
        r.lo = x.hi >> (shift - 64);
        r.hi = 0;
    } else {
        r.lo = 0;
        r.hi = 0;
    }
    return r;
}

// This structure holds the fraction f/2^s, so that multiplying by rhs is done as:
//   result = (f * rhs) >> s.
struct FreqFraction {
    // Allow use in a boolean context.
    explicit operator bool() const { return f != 0; }

    // Multiply by a 64-bit integer.
    uint64_t operator*(uint64_t rhs) const
    {
        U128 prod    = mul64(f, rhs);
        U128 shifted = u128_shr(prod, s);
        // We expect the result to fit in 64 bits.
        assert(shifted.hi == 0);
        return shifted.lo;
    }

    uint64_t f {0}; // numerator of the fraction
    int s {0};      // power-of-two divisor (i.e. the fraction is f/2^s)
};

inline FreqFraction computeFreqFraction(uint64_t num, uint64_t denom)
{
    int s = 63 - ceil_log2(num);
    // (num << s) must not overflow; we assume num is small enough.
    assert(s >= 0);
    uint64_t f = (num << s) / denom;
    return FreqFraction {f, s};
}