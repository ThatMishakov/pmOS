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
#include "../memory/malloc.hh"

#include <stddef.h>
#include <stdint.h>

template<typename T> class Circ_Array
{
private:
    static const size_t default_size = 16;
    T **ptr_array;
    size_t array_size;
    size_t index_start;
    size_t elements;

public:
    Circ_Array();
    Circ_Array(size_t);
    Circ_Array(const Circ_Array &);
    Circ_Array(size_t, T &);
    ~Circ_Array();

    size_t size() const;
    T &operator[](size_t n);
    const T &operator[](size_t n) const;

    T &front();
    const T &front() const;

    T &back();
    const T &back() const;

    T *data();
    const T *data() const;

    void push_back(const T &);
    void pop_back();

    void push_front(const T &);
    void pop_front();

    void clear();

    void reserve();

    inline bool empty() const { return size() == 0; }
};

template<typename T> size_t Circ_Array<T>::size() const { return elements; }

template<typename T> Circ_Array<t>::~Circ_Array() { delete[] ptr_array; }

Circ_Array() {}