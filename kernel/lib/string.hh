#pragma once
#include <stddef.h>
#include <stdint.h>
#include <utils.hh>
#include "utility.hh"
#include "initializer_list"
#include "pair.hh"
#include <types.hh>
#include "memory.hh"
#include "kernel/errors.h"

namespace klib {

class string {
private:
    struct long_string_str {
        size_t size = 0;
        char*  ptr = nullptr;
    };

    size_t s_capacity = 0;
    union {
        long_string_str long_string = {0,0};
        char short_string[16];
    };

    static constexpr size_t small_size = sizeof(long_string_str) - 1;

    static constexpr bool fits_in_short(size_t s)
    {
        return s <= small_size;
    }

    constexpr bool is_long() const noexcept
    {
        return not fits_in_short(s_capacity);
    }

    constexpr char* mutable_data() noexcept
    {
        return is_long() ? long_string.ptr : short_string;
    }

    constexpr void set_size(size_t new_size) noexcept
    {
        if (is_long())
            long_string.size = new_size;
        else
            s_capacity = new_size;
    }
public:
    static const size_t npos = -1;

    constexpr string() noexcept: s_capacity(0), long_string({0,0}) {};
    string(const string& str)
    {
        size_t length = str.length();

        if (length > small_size) {
            long_string.ptr = new char[length + 1];
            s_capacity = length;
            long_string.size = length;

            memcpy(long_string.ptr, str.data(), length);
            long_string.ptr[length] = '\0';
        } else {
            s_capacity = length;
            long_string = str.long_string;
        }

    }

    string (const string& str, size_t pos, size_t len = npos);

    string (const char* s)
    {
        size_t length = strlen(s);

        if (length > small_size) {
            long_string.ptr = new char[length+1];
            s_capacity = length;
            long_string.size = length;

            memcpy(long_string.ptr, s, length);
            long_string.ptr[length] = '\0';
        } else {
            s_capacity = length;
            memcpy(short_string, s, length);
            short_string[length] = '\0';
        }
    }
    string (const char* s, size_t n);
    string (size_t n, char c);
    // string (std::initializer_list<char> il);

    constexpr string(string&& str) noexcept: s_capacity(str.s_capacity), long_string(str.long_string)
        {
            str.s_capacity = 0;
            str.long_string = {0,0};
        }

    ~string() noexcept
    {
        if (is_long()) {
            delete[] long_string.ptr;
        }
    }

    constexpr const string& operator=(const string& str);
    string& operator= (const char* s)
    {
        *this = string(s);
        return *this;
    }
    string& operator= (char c);
    // string& operator= (std::initializer_list<char> il);
    const string& operator=(string&& str) noexcept
    {
        this->~string();

        this->s_capacity = str.s_capacity;
        this->long_string = str.long_string;

        str.s_capacity = 0;
        str.long_string = {0,0};

        return *this;
    }

    constexpr size_t size() const noexcept
    {
        return is_long() ? long_string.size : s_capacity;
    }

    constexpr size_t length() const noexcept
    {
        return size();
    }

    void resize (size_t n);
    void resize (size_t n, char c);

    constexpr size_t capacity() const noexcept
    {
        return is_long() ? s_capacity : small_size;
    }

    void reserve (size_t n = 0)
    {
        if (n <= capacity())
            return;

        if (is_long()) {
            char* new_ptr = new char[n+1];
            char* old_ptr = long_string.ptr;

            for (size_t i = 0; i < size(); ++i) {
                new_ptr[i] = old_ptr[i];
            }

            new_ptr[size()] = '\0';

            s_capacity = n;
            long_string.size = n;
            long_string.ptr = new_ptr;

            delete[] old_ptr;
        } else {
            if (fits_in_short(n))
                return;

            char* new_ptr = new char[n+1];

            for (size_t i = 0; i < size(); ++i) {
                new_ptr[i] = short_string[i];
            }
            new_ptr[size()] = '\0';

            s_capacity = n;
            long_string.size = n;
            long_string.ptr = new_ptr;
        }
    }

    void clear() noexcept
    {
        if (is_long())
            long_string.size = 0;
        else
            s_capacity = 0;
    }

    constexpr bool empty() const noexcept
    {
        return size() == 0;
    }

    void shrink_to_fit();

    char& operator[] (size_t pos);
    const char& operator[] (size_t pos) const;

    char& at (size_t pos);
    const char& at (size_t pos) const;

    char& back();
    const char& back() const;

    char& front();
    const char& front() const;

    constexpr const char* data() const noexcept
    {
        return is_long() ? long_string.ptr : short_string;
    }

    constexpr const char* c_str() const noexcept
    {
        return data();
    }

    size_t copy (char* s, size_t len, size_t pos = 0) const;

    string substr (size_t pos = 0, size_t len = npos) const;

    string& append (const string& str, size_t subpos, size_t sublen = npos);

    string& append (const char* s, size_t n)
    {
        size_t current_size = size();
        size_t str_size = n;
        size_t new_size = current_size + str_size;

        reserve(new_size);

        char* to = mutable_data();
        const char* from = s;

        for (size_t i = 0; i < str_size; ++i) {
            to[current_size + i] = from[i];
        }
        to[new_size] = '\0';

        set_size(new_size);

        return *this;
    }

    string& append (const string& str)
    {
        return append(str.data(), str.size());
    }

    string& append (size_t n, char c)
    {
        size_t current_size = size();
        size_t str_size = n;
        size_t new_size = current_size + str_size;

        if (new_size > capacity())
            reserve(capacity()*2);

        char* to = mutable_data();

        for (size_t i = 0; i < str_size; ++i) {
            to[current_size + i] = c;
        }
        to[new_size] = '\0';

        set_size(new_size);

        return *this;
    }

    string& append (const char* s)
    {
        return append(s, strlen(s));
    }

    string& operator+= (const string& str)
    {
        return append(str);
    }

    string& operator+= (const char* s)
    {
        return append(s);
    }

    string& operator+= (char c)
    {
        return append(1, c);
    }

    string& assign (const string& str);
    string& assign (const string& str, size_t subpos, size_t sublen = npos);
    string& assign (const char* s);
    string& assign (const char* s, size_t n);
    string& assign (size_t n, char c);

    void swap (string& str) noexcept
    {
        string tmp;
        tmp = forward<string>(*this);
        *this = forward<string>(str);
        str = forward<string>(tmp);
    }

    void pop_back();

    int compare (const string& str) const noexcept
    {
        size_t size = min(this->size(), str.size());

        const char* str1_data = this->data();
        const char* str2_data = str.data();

        for (size_t i = 0; i < size; ++i) {
            if (str1_data[i] != str2_data[i])
                return (int)str2_data[i] - (int)str1_data[i];
        }

        return (long)str.size() - (long)this->size();
    }

    int compare (size_t pos, size_t len, const string& str) const;
    int compare (size_t pos, size_t len, const string& str, size_t subpos, size_t sublen = npos) const;
    int compare (const char* s) const;int compare (size_t pos, size_t len, const char* s) const;
    int compare (size_t pos, size_t len, const char* s, size_t n) const;

    bool operator<(const string& s) const
    {
        return compare(s) < 0;
    }

    bool operator>(const string& s) const
    {
        return compare(s) > 0;
    }

    bool operator==(const string& s) const
    {
        return compare(s) == 0;
    }

    static klib::pair<bool, string> fill_from_user(const char* ptr, size_t size)
    {

        klib::pair<bool, string> ret;

        if (fits_in_short(size)) {
            ret.first = copy_from_user(ret.second.short_string, ptr, size);

            if (not ret.first)
                return ret;

            ret.second.s_capacity = size;
        } else {
            unique_ptr<char[]> p(new char[size+1]);

            ret.first = copy_from_user(p.get(), ptr, size);

            if (not ret.first)
                return ret;

            p[size] = '\0';

            ret.second.s_capacity = size;
            ret.second.long_string.size = size;
            ret.second.long_string.ptr = p.release();
        }

        return ret;
    }
};

}