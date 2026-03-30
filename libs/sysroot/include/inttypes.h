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

#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h>

#define __DECLARE_WCHAR_T
#include "__posix_types.h"

/// Structure for the result of the imaxdiv() function.
typedef struct {
    intmax_t quot; //< Quotient.
    intmax_t rem;  //< Remainder.
} imaxdiv_t;

// Detect if 'long' is 64 bits
#if __SIZEOF_LONG__ == 8
    #define PRId64 "ld"
    #define PRIi64 "li"
    #define PRIo64 "lo"
    #define PRIu64 "lu"
    #define PRIx64 "lx"
    #define PRIX64 "lX"

    #define SCNd64 "ld"
    #define SCNi64 "li"
    #define SCNo64 "lo"
    #define SCNu64 "lu"
    #define SCNx64 "lx"
#else
    // 'long' is 32 bits; use 'long long' for 64-bit integers
    #define PRId64 "lld"
    #define PRIi64 "lli"
    #define PRIo64 "llo"
    #define PRIu64 "llu"
    #define PRIx64 "llx"
    #define PRIX64 "llX"

    #define SCNd64 "lld"
    #define SCNi64 "lli"
    #define SCNo64 "llo"
    #define SCNu64 "llu"
    #define SCNx64 "llx"
#endif

// The rest of your format specifiers
#define PRId8  "d"
#define PRId16 "d"
#define PRId32 "d"

#define PRIi8  "i"
#define PRIi16 "i"
#define PRIi32 "i"

#define PRIo8  "o"
#define PRIo16 "o"
#define PRIo32 "o"

#define PRIu8  "u"
#define PRIu16 "u"
#define PRIu32 "u"

#define PRIx8  "x"
#define PRIx16 "x"
#define PRIx32 "x"

#define PRIX8  "X"
#define PRIX16 "X"
#define PRIX32 "X"

// Similarly for scanf format specifiers
#define SCNd8  "hhd"
#define SCNd16 "hd"
#define SCNd32 "d"

#define SCNi8  "hhi"
#define SCNi16 "hi"
#define SCNi32 "i"

#define SCNo8  "hho"
#define SCNo16 "ho"
#define SCNo32 "o"

#define SCNu8  "hhu"
#define SCNu16 "hu"
#define SCNu32 "u"

#define SCNx8  "hhx"
#define SCNx16 "hx"
#define SCNx32 "x"

// Define macros for intmax_t and intptr_t
#if __SIZEOF_LONG__ == 8
    #define PRIdMAX "ld"
    #define PRIiMAX "li"
    #define PRIoMAX "lo"
    #define PRIuMAX "lu"
    #define PRIxMAX "lx"
    #define PRIXMAX "lX"

    #define SCNdMAX "ld"
    #define SCNiMAX "li"
    #define SCNoMAX "lo"
    #define SCNuMAX "lu"
    #define SCNxMAX "lx"

    #define PRIdPTR "ld"
    #define PRIiPTR "li"
    #define PRIoPTR "lo"
    #define PRIuPTR "lu"
    #define PRIxPTR "lx"
    #define PRIXPTR "lX"

    #define SCNdPTR "ld"
    #define SCNiPTR "li"
    #define SCNoPTR "lo"
    #define SCNuPTR "lu"
    #define SCNxPTR "lx"
#else
    #define PRIdMAX "lld"
    #define PRIiMAX "lli"
    #define PRIoMAX "llo"
    #define PRIuMAX "llu"
    #define PRIxMAX "llx"
    #define PRIXMAX "llX"

    #define SCNdMAX "lld"
    #define SCNiMAX "lli"
    #define SCNoMAX "llo"
    #define SCNuMAX "llu"
    #define SCNxMAX "llx"

    #define PRIdPTR "d"
    #define PRIiPTR "i"
    #define PRIoPTR "o"
    #define PRIuPTR "u"
    #define PRIxPTR "x"
    #define PRIXPTR "X"

    #define SCNdPTR "d"
    #define SCNiPTR "i"
    #define SCNoPTR "o"
    #define SCNuPTR "u"
    #define SCNxPTR "x"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

intmax_t imaxabs(intmax_t);
imaxdiv_t imaxdiv(intmax_t, intmax_t);
intmax_t strtoimax(const char *, char **, int);
uintmax_t strtoumax(const char *, char **, int);
intmax_t wcstoimax(const wchar_t *, wchar_t **, int);
uintmax_t wcstoumax(const wchar_t *, wchar_t **, int);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _INTTYPES_H