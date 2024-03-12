#pragma once
#include "type_traits.hh"

namespace klib {

template< class T >
constexpr remove_reference_t<T>&& move( T&& t ) noexcept
{
    return static_cast<remove_reference_t<T>&&>(t);
}

template< class T >
constexpr T&& forward( remove_reference_t<T>& t ) noexcept
{
    return static_cast<T&&>(t);
}

template< class T >
constexpr T&& forward( remove_reference_t<T>&& t ) noexcept
{
    static_assert(!is_lvalue_reference_v<T>, "Not lvalue reference\n");
    return static_cast<T&&>(t);

}

template< class T >
constexpr void swap( T& a, T& b )
{
    T temp(move(a));
    a = move(b);
    b = move(temp);
}
}