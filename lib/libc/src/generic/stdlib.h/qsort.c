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

#include <stdlib.h>
#include <string.h>

static void swap(void *a, void *b, size_t size)
{
    unsigned char t[size];

    memcpy(t, a, size);
    memcpy(a, b, size);
    memcpy(b, t, size);
}

static size_t partition(void *base, size_t elem_size, size_t l, size_t r, int (*compar)(const void *, const void *))
{
    void *pivot = base + r * elem_size;
    size_t i = l;

    for (size_t j = l; j < r; j++)
    {
        if (compar(base + j * elem_size, pivot) < 0)
        {
            swap(base + i * elem_size, base + j * elem_size, elem_size);
            i++;
        }
    }

    swap(base + i * elem_size, pivot, elem_size);
    return i;
}

static void qsort_i(void *base, size_t elem_size, size_t l, size_t r, int (*compar)(const void *, const void *))
{
    size_t p = partition(base, elem_size, l, r, compar);

    if (p > l+1)
        qsort_i(base, elem_size, l, p - 1, compar);
    if (p+1 < r)
        qsort_i(base, elem_size, p + 1, r, compar);
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
    if (nmemb > 2)
        qsort_i(base, size, 0, nmemb - 1, compar);
}

