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
#include "initializer_list"
#include "memory.hh"
#include "pair.hh"
#include "utility.hh"

#include <stddef.h>
#include <stdint.h>
#include <types.hh>
#include <utils.hh>
#include <errno.h>

namespace klib
{

class string
{
private:
    struct long_string_str {
        size_t size = 0;
        char *ptr   = nullptr;
    };

    size_t s_capacity = 0;
    union {
        long_string_str long_string = {0, 0};
        char short_string[16];
    };

    static constexpr size_t small_size = sizeof(long_string_str) - 1;

    static constexpr bool fits_in_short(size_t s) { return s <= small_size; }

    constexpr bool is_long() const noexcept { return not fits_in_short(s_capacity); }

    constexpr char *mutable_data() noexcept { return is_long() ? long_string.ptr : short_string; }

    constexpr void set_size(size_t new_size) noexcept
    {
        if (is_long())
            long_string.size = new_size;
        else
            s_capacity = new_size;
    }

public:
    static const size_t npos = -1;

    constexpr string() noexcept: s_capacity(0), long_string({0, 0}) {};
    string(const string &str)
    {
        size_t length = str.length();

        if (length > small_size) {
            long_string.ptr  = new char[length + 1];
            s_capacity       = length;
            long_string.size = length;

            memcpy(long_string.ptr, str.data(), length);
            long_string.ptr[length] = '\0';
        } else {
            s_capacity  = length;
            long_string = str.long_string;
        }
    }

    string(const string &str, size_t pos, size_t len = npos);

    string(const char *s)
    {
        size_t length = strlen(s);

        if (length > small_size) {
            long_string.ptr  = new char[length + 1];
            if (long_string.ptr)
                return;

            s_capacity       = length;
            long_string.size = length;

            memcpy(long_string.ptr, s, length);
            long_string.ptr[length] = '\0';
        } else {
            s_capacity = length;
            memcpy(short_string, s, length);
            short_string[length] = '\0';
        }
    }

    string(const char *s, size_t n)
    {
        if (n > small_size) {
            long_string.ptr  = new char[n + 1];
            if (!long_string.ptr)
                return;

            s_capacity       = n;
            long_string.size = n;

            memcpy(long_string.ptr, s, n);
            long_string.ptr[n] = '\0';
        } else {
            s_capacity = n;
            memcpy(short_string, s, n);
            short_string[n] = '\0';
        }
    }

    string(size_t n, char c)
    {
        if (n > small_size) {
            long_string.ptr  = new char[n + 1];
            if (long_string.ptr)
                return;

            s_capacity       = n;
            long_string.size = n;

            for (size_t i = 0; i < n; ++i) {
                long_string.ptr[i] = c;
            }
            long_string.ptr[n] = '\0';
        } else {
            s_capacity = n;
            for (size_t i = 0; i < n; ++i) {
                short_string[i] = c;
            }
            short_string[n] = '\0';
        }
    }

    constexpr string(string &&str) noexcept
        : s_capacity(str.s_capacity), long_string(str.long_string)
    {
        str.s_capacity  = 0;
        str.long_string = {0, 0};
    }

    ~string() noexcept
    {
        if (is_long()) {
            delete[] long_string.ptr;
        }
    }

    string &operator=(char c);
    // string& operator= (std::initializer_list<char> il);
    const string &operator=(string &&str) noexcept
    {
        this->~string();

        this->s_capacity  = str.s_capacity;
        this->long_string = str.long_string;

        str.s_capacity  = 0;
        str.long_string = {0, 0};

        return *this;
    }

    constexpr size_t size() const noexcept { return is_long() ? long_string.size : s_capacity; }

    constexpr size_t length() const noexcept { return size(); }

    void resize(size_t n);
    void resize(size_t n, char c);

    constexpr size_t capacity() const noexcept { return is_long() ? s_capacity : small_size; }

    void reserve(size_t n = 0)
    {
        if (n <= capacity())
            return;

        if (is_long()) {
            char *new_ptr = new char[n + 1];
            char *old_ptr = long_string.ptr;

            for (size_t i = 0; i < size(); ++i) {
                new_ptr[i] = old_ptr[i];
            }

            new_ptr[size()] = '\0';

            s_capacity       = n;
            long_string.size = n;
            long_string.ptr  = new_ptr;

            delete[] old_ptr;
        } else {
            if (fits_in_short(n))
                return;

            char *new_ptr = new char[n + 1];

            for (size_t i = 0; i < size(); ++i) {
                new_ptr[i] = short_string[i];
            }
            new_ptr[size()] = '\0';

            s_capacity       = n;
            long_string.size = n;
            long_string.ptr  = new_ptr;
        }
    }

    void clear() noexcept
    {
        if (is_long())
            long_string.size = 0;
        else
            s_capacity = 0;
    }

    constexpr bool empty() const noexcept { return size() == 0; }

    void shrink_to_fit();

    char &operator[](size_t pos);
    const char &operator[](size_t pos) const;

    char &at(size_t pos);
    const char &at(size_t pos) const;

    char &back();
    const char &back() const;

    char &front();
    const char &front() const;

    constexpr const char *data() const noexcept
    {
        return is_long() ? long_string.ptr : short_string;
    }

    constexpr const char *c_str() const noexcept { return data(); }

    size_t copy(char *s, size_t len, size_t pos = 0) const;

    string substr(size_t pos = 0, size_t len = npos) const;

    string &append(const string &str, size_t subpos, size_t sublen = npos);

    string &append(const char *s, size_t n)
    {
        size_t current_size = size();
        size_t str_size     = n;
        size_t new_size     = current_size + str_size;

        reserve(new_size);

        char *to         = mutable_data();
        const char *from = s;

        for (size_t i = 0; i < str_size; ++i) {
            to[current_size + i] = from[i];
        }
        to[new_size] = '\0';

        set_size(new_size);

        return *this;
    }

    string &append(const string &str) { return append(str.data(), str.size()); }

    string &append(size_t n, char c)
    {
        size_t current_size = size();
        size_t str_size     = n;
        size_t new_size     = current_size + str_size;

        if (new_size > capacity())
            reserve(capacity() * 2);

        char *to = mutable_data();

        for (size_t i = 0; i < str_size; ++i) {
            to[current_size + i] = c;
        }
        to[new_size] = '\0';

        set_size(new_size);

        return *this;
    }

    string &append(const char *s) { return append(s, strlen(s)); }

    string &operator+=(const string &str) { return append(str); }

    string &operator+=(const char *s) { return append(s); }

    string &operator+=(char c) { return append(1, c); }

    string operator+(const string &str) const
    {
        string ret(*this);
        ret.append(str);
        return ret;
    }

    string operator+(const char *s) const
    {
        string ret(*this);
        ret.append(s);
        return ret;
    }

    string operator+(char c) const
    {
        string ret(*this);
        ret.append(1, c);
        return ret;
    }

    string &assign(const string &str);
    string &assign(const string &str, size_t subpos, size_t sublen = npos);
    string &assign(const char *s);
    string &assign(const char *s, size_t n);
    string &assign(size_t n, char c);

    void swap(string &str) noexcept
    {
        string tmp;
        tmp   = forward<string>(*this);
        *this = forward<string>(str);
        str   = forward<string>(tmp);
    }

    void pop_back();

    int compare(const string &str) const noexcept
    {
        size_t size = min(this->size(), str.size());

        const char *str1_data = this->data();
        const char *str2_data = str.data();

        for (size_t i = 0; i < size; ++i) {
            if (str1_data[i] != str2_data[i])
                return (int)str2_data[i] - (int)str1_data[i];
        }

        return (long)str.size() - (long)this->size();
    }

    int compare(size_t pos, size_t len, const string &str) const;
    int compare(size_t pos, size_t len, const string &str, size_t subpos,
                size_t sublen = npos) const;
    int compare(const char *s) const;
    int compare(size_t pos, size_t len, const char *s) const;
    int compare(size_t pos, size_t len, const char *s, size_t n) const;

    bool operator<(const string &s) const { return compare(s) < 0; }
    bool operator>(const string &s) const { return compare(s) > 0; }
    bool operator==(const string &s) const { return compare(s) == 0; }

    string clone() const { return string(*this); }
};

} // namespace klib