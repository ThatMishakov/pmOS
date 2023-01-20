#pragma once
#include <stddef.h>

namespace klib
{
    
template<class T, size_t N>
struct array {
    T elem[N];
public:
    using size_type = size_t;

    T& operator[](size_t p)
    {
        return elem[p];
    }

    const T& operator[](size_t p) const
    {
        return elem[p];
    }

    constexpr size_type size() const noexcept
    {
        return N;
    }
};

} // namespace klib
