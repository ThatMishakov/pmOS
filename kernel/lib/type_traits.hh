#pragma once

// Read cppreference.com

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

template<typename T>
struct is_union : public integral_constant<bool, __is_union(T)>
{ };

namespace klib_is_class_details {
template <class T>
klib::integral_constant<bool, !klib::is_union<T>::value> test(int T::*);
 
template <class>
klib::false_type test(...);
}
 
template <class T>
struct is_class : decltype(klib_is_class_details::test<T>(nullptr))
{};

namespace klib_is_base_of_details {
    template <typename B>
    klib::true_type test_pre_ptr_convertible(const volatile B*);
    template <typename>
    klib::false_type test_pre_ptr_convertible(const volatile void*);
 
    template <typename, typename>
    auto test_pre_is_base_of(...) -> klib::true_type;
    template <typename B, typename D>
    auto test_pre_is_base_of(int) ->
        decltype(test_pre_ptr_convertible<B>(static_cast<D*>(nullptr)));
} // namespace klib_details


template <typename Base, typename Derived>
struct is_base_of :
    klib::integral_constant<
        bool,
        klib::is_class<Base>::value && klib::is_class<Derived>::value &&
        decltype(klib_is_base_of_details::test_pre_is_base_of<Base, Derived>(0))::value
    > { };
} // namespace klib