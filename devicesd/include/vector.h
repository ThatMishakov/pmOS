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

#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#define VECTOR(type)     \
    struct {             \
        type *data;      \
        size_t size;     \
        size_t capacity; \
    }

#define VECTOR_TYPEDEF(type, name) typedef VECTOR(type) name

#define VECTOR_INIT \
    {               \
        NULL, 0, 0  \
    }

#define VECTOR_PUSH_BACK(vec, value)                                                      \
    do {                                                                                  \
        if ((vec).size == (vec).capacity) {                                               \
            (vec).capacity = (vec).capacity == 0 ? 1 : (vec).capacity * 2;                \
            (vec).data     = realloc((vec).data, (vec).capacity * sizeof((vec).data[0])); \
        }                                                                                 \
        (vec).data[(vec).size++] = value;                                                 \
    } while (0)

#define VECTOR_PUSH_BACK_CHECKED(vec, value, result)                                      \
    do {                                                                                  \
        if ((vec).size == (vec).capacity) {                                               \
            (vec).capacity = (vec).capacity == 0 ? 4 : (vec).capacity * 2;                \
            void *new_data = realloc((vec).data, (vec).capacity * sizeof((vec).data[0])); \
            if (!new_data) {                                                              \
                result = -1;                                                              \
                break;                                                                    \
            }                                                                             \
            (vec).data = new_data;                                                        \
        }                                                                                 \
        (vec).data[(vec).size++] = value;                                                 \
        result                   = 0;                                                     \
    } while (0)

#define VECTOR_FOREACH(vec, var) \
    for (size_t i = 0; i < (vec).size && ((var) = (vec).data[i], 1); ++i)

#define VECTOR_FREE(vec)       \
    do {                       \
        free((vec).data);      \
        (vec).data     = NULL; \
        (vec).size     = 0;    \
        (vec).capacity = 0;    \
    } while (0)

#define VECTOR_RESERVE(vec, count, result)                            \
    do {                                                              \
        if ((vec).capacity >= count)                                      \
            result = 0;                                               \
        else {                                                        \
            void *data = realloc((vec).data, count*sizeof(*(vec).data)); \
            if (!data)                                                \
                result = -1;                                          \
            else {                                                    \
                result = 0;                                           \
                (vec).data = data;                                     \
                (vec).capacity = count;                               \
            }                                                         \
        }                                                             \
    } while (0)


#define VECTOR_SIZE(vec)  ((vec).size)
#define VECTOR_EMPTY(vec) ((vec).size == 0)

#define VECTOR_SORT(vec, cmp) qsort((vec).data, (vec).size, sizeof((vec).data[0]), cmp)
#define VECTOR_BSEARCH(vec, key, cmp) \
    bsearch(&(key), (vec).data, (vec).size, sizeof((vec).data[0]), cmp)

#endif // VECTOR_H