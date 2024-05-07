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

#include "string.h"

#include <stdlib.h>
#include <string.h>

int init_null_string(struct String *string)
{
    string->data     = NULL;
    string->length   = 0;
    string->capacity = 0;
    return 0;
}

void destroy_string(struct String *string)
{
    if (string->data != NULL)
        free(string->data);

    string->data     = NULL;
    string->length   = 0;
    string->capacity = 0;
}

int append_string(struct String *string, const char *data, size_t length)
{
    size_t required_capacity = string->length + length + 1;
    if (required_capacity > string->capacity) {
        size_t new_capacity =
            (required_capacity > string->capacity * 2) ? required_capacity : string->capacity * 2;
        char *new_data = (char *)realloc(string->data, new_capacity);
        if (new_data == NULL)
            return -1;
        string->data     = new_data;
        string->capacity = new_capacity;
    }
    memcpy(string->data + string->length, data, length);
    string->length += length;
    string->data[string->length] = '\0';
    return 0;
}

void erase_string(struct String *string, size_t start, size_t count)
{
    if (start >= string->length)
        return;
    if (start + count > string->length)
        count = string->length - start;
    memmove(string->data + start, string->data + start + count, string->length - start - count);
    string->length -= count;
    string->data[string->length] = '\0';
}