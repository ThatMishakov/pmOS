#pragma once
#include <stddef.h>
namespace klib
{
    
template<class T, size_t N>
struct array {
    T elem[N];

    using size_type = size_t;

    constexpr T& operator[](size_t p)
    {
        return elem[p];
    }

    constexpr const T& operator[](size_t p) const
    {
        return elem[p];
    }

    constexpr size_type size() const noexcept
    {
        return N;
    }

    class iterator {
        friend array;
    private:
        T* ptr;
        constexpr iterator(T* n): ptr(n) {};

    public:
        iterator& operator++() {
            ptr++;
            return *this;
        }

        T& operator*()
        {
            return *ptr;
        }

        bool operator==(iterator k)
        {
            return this->ptr == k.ptr;
        }
    };

    constexpr iterator begin() noexcept
    {
        return &elem[0];
    }

    constexpr iterator end() noexcept
    {
        return &elem[N];
    }
};

} // namespace klib
