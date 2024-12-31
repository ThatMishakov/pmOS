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
#include "utility.hh"

#include <memory/malloc.hh>
#include <stddef.h>

namespace klib
{

template<typename T> class vector
{
private:
    static const size_t start_size = 16;

    T *ptr            = nullptr;
    size_t a_capacity = 0;
    size_t a_size     = 0;

    bool expand(size_t to);
    size_t next_capacity() const;

public:
    typedef T value_type;
    typedef const T &const_reference;
    typedef size_t size_type;

    class iterator
    {
    private:
        T *ptr;

    public:
        constexpr iterator(T *n): ptr(n) {};

        iterator &operator++()
        {
            ptr++;
            return *this;
        }

        iterator operator++(int)
        {
            iterator temp = *this;
            ptr++;
            return temp;
        }

        iterator &operator--()
        {
            ptr--;
            return *this;
        }

        iterator operator--(int)
        {
            iterator temp = *this;
            ptr--;
            return temp;
        }

        iterator &operator+=(size_t n)
        {
            ptr += n;
            return *this;
        }

        iterator &operator-=(size_t n)
        {
            ptr -= n;
            return *this;
        }

        long operator-(iterator k) const { return this->ptr - k.ptr; }

        iterator operator+(long n) const { return iterator(ptr + n); }
        iterator operator-(long n) const { return iterator(ptr - n); }

        bool operator!=(iterator k) const { return this->ptr != k.ptr; }
        bool operator<(iterator k) const { return this->ptr < k.ptr; }

        T &operator*() { return *ptr; }
        const T &operator*() const { return *ptr; }

        T *operator->() { return ptr; }

        bool operator==(iterator k) const { return this->ptr == k.ptr; }
    };

    class const_iterator
    {
    private:
        const T *ptr;

    public:
        constexpr const_iterator(const T *n): ptr(n) {};

        const_iterator &operator++()
        {
            ptr++;
            return *this;
        }

        const T &operator*() const { return *ptr; }

        bool operator==(const_iterator k) const { return this->ptr == k.ptr; }

        const_iterator &operator++(int) { return *ptr++; }
    };

    vector() noexcept;
    vector(vector<T> &&) noexcept;
    ~vector() noexcept;

    vector<T> &operator=(vector<T> &&from) noexcept;
    T &operator*();
    const T &operator*() const;

    T &operator[](size_t);
    const T &operator[](size_t) const;

    size_t size() const;

    T &front();
    const T &front() const;

    T &back();
    const T &back() const;

    T *data();
    const T *data() const;

    bool push_back(const T &) noexcept;
    bool push_back(T &&) noexcept;
    bool emplace_back(T &&) noexcept;
    void pop_back() noexcept;

    void clear() noexcept;

    [[nodiscard]] bool reserve(size_t) noexcept;

    inline bool empty() const { return a_size == 0; }

    iterator begin();
    iterator end();

    const_iterator begin() const;
    const_iterator end() const;

    [[nodiscard]] bool resize(size_type n) noexcept;
    [[nodiscard]] bool resize(size_type n, const value_type &val) noexcept;

    size_t capacity() const { return a_capacity; }
};

template<typename T> vector<T>::vector() noexcept: ptr(nullptr), a_capacity(0), a_size(0) {};

template<typename T> size_t vector<T>::next_capacity() const
{
    if (a_capacity < start_size)
        return start_size;

    return a_capacity * 2;
}

template<typename T> vector<T>::~vector() noexcept
{
    for (size_t i = 0; i < a_size; ++i) {
        ptr[i].~T();
    }

    free(ptr);
}

template<typename T> vector<T>::vector(vector<T> &&from) noexcept
{
    ptr        = from.ptr;
    a_capacity = from.a_capacity;
    a_size     = from.a_size;

    from.ptr        = nullptr;
    from.a_capacity = 0;
    from.a_size     = 0;
}

template<typename T> vector<T> &vector<T>::operator=(vector<T> &&from) noexcept
{
    if (this != &from) {
        for (size_t i = 0; i < a_size; ++i) {
            ptr[i].~T();
        }
        free(ptr);

        ptr        = from.ptr;
        a_capacity = from.a_capacity;
        a_size     = from.a_size;

        from.ptr        = nullptr;
        from.a_capacity = 0;
        from.a_size     = 0;
    }
    return *this;
}

template<typename T> T &vector<T>::operator*() { return *ptr; }

template<typename T> const T &vector<T>::operator*() const { return *ptr; }

template<typename T> T &vector<T>::front() { return *ptr; }

template<typename T> const T &vector<T>::front() const { return *ptr; }

template<typename T> inline size_t vector<T>::size() const { return a_size; }

template<typename T> typename vector<T>::iterator vector<T>::begin() { return iterator(ptr); }

template<typename T> typename vector<T>::iterator vector<T>::end()
{
    return iterator(ptr + a_size);
}

template<typename T> typename vector<T>::const_iterator vector<T>::begin() const
{
    return const_iterator(ptr);
}

template<typename T> typename vector<T>::const_iterator vector<T>::end() const
{
    return const_iterator(ptr + a_size);
}

template<typename T> const T &vector<T>::operator[](size_t p) const { return ptr[p]; }

template<typename T> T &vector<T>::operator[](size_t p) { return ptr[p]; }

template<typename T> bool vector<T>::push_back(T &&p) noexcept
{
    if ((a_size >= a_capacity) && !expand(next_capacity()))
        return false;

    new (&ptr[a_size++]) T(forward<T>(p));
    return true;
}

template<typename T> bool vector<T>::resize(size_t new_size) noexcept
{
    if (a_size > new_size) {
        for (auto i = a_size; i > new_size; --i) {
            ptr[i - 1].~T();
        }
        a_size = new_size;
    } else if (a_size < new_size) {
        if ((new_size > a_capacity) && !expand(new_size))
            return false;

        auto i = a_size;

        for (; i < new_size; ++i)
            new (&ptr[i]) T();

        a_size = new_size;
    }

    return true;
}

template<typename T> bool vector<T>::resize(size_t new_size, const T &t) noexcept
{
    if (a_size > new_size) {
        for (auto i = a_size; i > new_size; --i) {
            ptr[i - 1].~T();
        }
        a_size = new_size;
    } else if (a_size < new_size) {
        if ((new_size > a_capacity) && !expand(new_size))
            return false;

        auto i = a_size;

        for (; i < new_size; ++i)
            new (&ptr[i]) T(t);

        a_size = new_size;
    }

    return true;
}

template<typename T> bool vector<T>::reserve(size_t new_capacity) noexcept
{
    if (new_capacity <= a_capacity)
        return true;

    return expand(new_capacity);
}

template<typename T> bool vector<T>::push_back(const T &p) noexcept
{
    if ((a_size >= a_capacity) && !expand(next_capacity()))
        return false;

    new (&ptr[a_size++]) T(p);
    return true;
}

template<typename T> bool vector<T>::emplace_back(T &&p) noexcept
{
    return push_back(forward<T>(p));
}

template<typename T> bool vector<T>::expand(size_t new_capacity)
{
    if (new_capacity <= a_capacity)
        return true;

    T *temp_ptr = (T *)malloc(sizeof(T) * new_capacity);
    if (!temp_ptr)
        return false;

    for (size_t i = 0; i < a_size; ++i)
        new (&temp_ptr[i]) T(move(ptr[i]));

    for (size_t i = 0; i < a_size; ++i)
        ptr[i].~T();

    a_capacity = new_capacity;
    free(ptr);
    ptr = temp_ptr;
    return true;
}

template<typename T> void vector<T>::pop_back() noexcept
{
    ptr[a_size - 1].~T();
    --a_size;
}

template<typename T> void vector<T>::clear() noexcept
{
    for (size_t i = 0; i < a_size; ++i)
        ptr[i].~T();

    a_size = 0;
}

template<typename T> T *vector<T>::data() { return ptr; }

template<typename T> const T *vector<T>::data() const { return ptr; }

} // namespace klib