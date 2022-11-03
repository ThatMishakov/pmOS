#pragma once
#include "type_traits.hh"

namespace klib {

template< class T >
constexpr remove_reference_t<T>&& move( T&& t ) noexcept;

template< class T >
constexpr T&& forward( remove_reference_t<T>& t ) noexcept;

template< class T >
constexpr T&& forward( remove_reference_t<T>&& t ) noexcept;

}