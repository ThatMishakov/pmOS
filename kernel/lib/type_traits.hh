#pragma once

namespace klib
{

template< class T > struct remove_reference      { typedef T type; };
template< class T > struct remove_reference<T&>  { typedef T type; };
template< class T > struct remove_reference<T&&> { typedef T type; };

template<class T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant; // using injected-class-name
    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; } // since c++14
};

template< class T >
using remove_reference_t = typename remove_reference<T>::type;

typedef integral_constant<bool,true> true_type;
typedef integral_constant<bool,false> false_type;

template<class T> struct is_lvalue_reference     : false_type {};
template<class T> struct is_lvalue_reference<T&> : true_type {};

template<class T>
constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

} // namespace klib
