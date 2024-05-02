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

#ifndef _STDINT_H
#define _STDINT_H

// uintN_t
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

// intN_t
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long int64_t;

// uint_leastN_t
typedef unsigned char uint_least8_t;
typedef unsigned short uint_least16_t;
typedef unsigned int uint_least32_t;
typedef unsigned long uint_least64_t;

// int_leastN_t
typedef signed char int_least8_t;
typedef signed short int_least16_t;
typedef signed int int_least32_t;
typedef signed long int_least64_t;

// uint_fastN_t
typedef unsigned int uint_fast8_t;
typedef unsigned int uint_fast16_t;
typedef unsigned int uint_fast32_t;
typedef unsigned long uint_fast64_t;

// int_fastN_t
typedef signed int int_fast8_t;
typedef signed int int_fast16_t;
typedef signed int int_fast32_t;
typedef signed long int_fast64_t;

typedef unsigned long uintptr_t;
typedef long intptr_t;

typedef long intmax_t;
typedef unsigned long uintmax_t;

#define INT8_MIN (-128)
#define INT16_MIN (-32768)
#define INT32_MIN (-2147483648)
#define INT64_MIN (-9223372036854775808L)

#define INT8_MAX 127
#define INT16_MAX 32767
#define INT32_MAX 2147483647
#define INT64_MAX 9223372036854775807L

#define UINT8_MAX 255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295U
#define UINT64_MAX 18446744073709551615UL

#define INT_LEAST8_MIN (-128)
#define INT_LEAST16_MIN (-32768)
#define INT_LEAST32_MIN (-2147483648)
#define INT_LEAST64_MIN (-9223372036854775808L)

#define INT_LEAST8_MAX 127
#define INT_LEAST16_MAX 32767
#define INT_LEAST32_MAX 2147483647
#define INT_LEAST64_MAX 9223372036854775807L

#define UINT_LEAST8_MAX 255
#define UINT_LEAST16_MAX 65535
#define UINT_LEAST32_MAX 4294967295U
#define UINT_LEAST64_MAX 18446744073709551615UL

#define INT_FAST8_MIN (-2147483648)
#define INT_FAST16_MIN (-2147483648)
#define INT_FAST32_MIN (-2147483648)
#define INT_FAST64_MIN (-9223372036854775808L)

#define INT_FAST8_MAX 2147483647
#define INT_FAST16_MAX 2147483647
#define INT_FAST32_MAX 2147483647
#define INT_FAST64_MAX 9223372036854775807L

#define UINT_FAST8_MAX 4294967295U
#define UINT_FAST16_MAX 4294967295U
#define UINT_FAST32_MAX 4294967295U
#define UINT_FAST64_MAX 18446744073709551615UL

#define INTPTR_MIN (-9223372036854775808L)
#define INTPTR_MAX 9223372036854775807L

#define UINTPTR_MAX 18446744073709551615UL

#define INTMAX_MIN (-9223372036854775808L)
#define INTMAX_MAX 9223372036854775807L

#define UINTMAX_MAX 18446744073709551615UL

#define PTRDIFF_MIN (-9223372036854775808L)
#define PTRDIFF_MAX 9223372036854775807L

// 64 bit
#define SIG_ATOMIC_MIN (-9223372036854775808L)
#define SIG_ATOMIC_MAX 9223372036854775807L

#define SIZE_MAX 18446744073709551615UL

#define WCHAR_MIN (-2147483648)
#define WCHAR_MAX (2147483647)

#define WINT_MIN 0
#define WINT_MAX 4294967295U

#define INT8_C(x) ((int8_t)x)
#define INT16_C(x) ((int16_t)x)
#define INT32_C(x) ((int32_t)x)
#define INT64_C(x) ((int64_t)x##L)

#define UINT8_C(x) ((uint8_t)x)
#define UINT16_C(x) ((uint16_t)x)
#define UINT32_C(x) ((uint32_t)x##U)
#define UINT64_C(x) ((uint64_t)x##UL)

#define INTMAX_C(x) ((intmax_t)x##L)
#define UINTMAX_C(x) ((uintmax_t)x##UL)

#endif // _STDINT_H