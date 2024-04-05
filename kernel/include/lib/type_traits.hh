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

// Read cppreference.com

namespace klib
{

typedef unsigned long size_t;

template< class T > struct remove_reference      { typedef T type; };
template< class T > struct remove_reference<T&>  { typedef T type; };
template< class T > struct remove_reference<T&&> { typedef T type; };

template<class T>
struct remove_extent { using type = T; }; 
template<class T>
struct remove_extent<T[]> { using type = T; };
template<class T, size_t N>
struct remove_extent<T[N]> { using type = T; };

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

template< class T > struct remove_cv                   { typedef T type; };
template< class T > struct remove_cv<const T>          { typedef T type; };
template< class T > struct remove_cv<volatile T>       { typedef T type; };
template< class T > struct remove_cv<const volatile T> { typedef T type; };
 
template< class T > struct remove_const                { typedef T type; };
template< class T > struct remove_const<const T>       { typedef T type; };
 
template< class T > struct remove_volatile             { typedef T type; };
template< class T > struct remove_volatile<volatile T> { typedef T type; };

typedef integral_constant<bool,true> true_type;
typedef integral_constant<bool,false> false_type;

template<bool B, class T, class F>
struct conditional { using type = T; };
 
template<class T, class F>
struct conditional<false, T, F> { using type = F; };

template< bool B, class T, class F >
using conditional_t = typename conditional<B,T,F>::type;

template<class...> struct conjunction : klib::true_type { };
template<class B1> struct conjunction<B1> : B1 { };
template<class B1, class... Bn>
struct conjunction<B1, Bn...>
    : klib::conditional_t<bool(B1::value), conjunction<Bn...>, B1> {};

template<class T> struct is_lvalue_reference     : false_type {};
template<class T> struct is_lvalue_reference<T&> : true_type {};

template<class T, class U>
struct is_same : klib::false_type {};
 
template<class T>
struct is_same<T, T> : klib::true_type {};

template< class T >
struct is_void : klib::is_same<void, typename klib::remove_cv<T>::type> {};

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

namespace add_lvalue_reference_detail {
 
template <class T>
struct type_identity { using type = T; }; // or use std::type_identity (since C++20)
 
template <class T> // Note that `cv void&` is a substitution failure
auto try_add_lvalue_reference(int) -> type_identity<T&>;
template <class T> // Handle T = cv void case
auto try_add_lvalue_reference(...) -> type_identity<T>;
 
template <class T>
auto try_add_rvalue_reference(int) -> type_identity<T&&>;
template <class T>
auto try_add_rvalue_reference(...) -> type_identity<T>;
 
} // namespace add_lvalue_reference_detail
 
template <class T>
struct add_lvalue_reference
    : decltype(add_lvalue_reference_detail::try_add_lvalue_reference<T>(0)) {};
 
template <class T>
struct add_rvalue_reference
    : decltype(add_lvalue_reference_detail::try_add_rvalue_reference<T>(0)) {};

template<typename T>
constexpr bool always_false = false;
 
template<typename T>
typename klib::add_rvalue_reference<T>::type declval() noexcept
{
    static_assert(always_false<T>, "declval not allowed in an evaluated context");
}

namespace is_convertible_detail {
 
template<class T>
auto test_returnable(int) -> decltype(
    void(static_cast<T(*)()>(nullptr)), klib::true_type{}
);
template<class>
auto test_returnable(...) -> klib::false_type;
 
template<class From, class To>
auto test_implicitly_convertible(int) -> decltype(
    void(klib::declval<void(&)(To)>()(klib::declval<From>())), klib::true_type{}
);
template<class, class>
auto test_implicitly_convertible(...) -> klib::false_type;
 
} // is_convertible_detail detail
 
template<class From, class To>
struct is_convertible : klib::integral_constant<bool,
    (decltype(is_convertible_detail::test_returnable<To>(0))::value &&
     decltype(is_convertible_detail::test_implicitly_convertible<From, To>(0))::value) ||
    (klib::is_void<From>::value && klib::is_void<To>::value)
> {};

template<class From, class To>
struct is_nothrow_convertible : klib::conjunction<klib::is_void<From>, klib::is_void<To>> {};
 
template<class From, class To>
    requires
        requires {
            static_cast<To(*)()>(nullptr);
            { klib::declval<void(&)(To) noexcept>()(klib::declval<From>()) } noexcept;
        }
struct is_nothrow_convertible<From, To> : klib::true_type {};

template < template <typename...> class base,typename derived>
struct is_base_of_template_impl
{
    template<typename... Ts>
    static constexpr klib::true_type  test(const base<Ts...> *);
    static constexpr klib::false_type test(...);
    using type = decltype(test(klib::declval<derived*>()));
};

template < template <typename...> class base,typename derived>
using is_base_of_template = typename is_base_of_template_impl<base,derived>::type;

} // namespace klib