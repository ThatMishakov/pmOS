#pragma once

// This should be in #include <utility>, but since the kernel's headers are a mess, it is broken

namespace pmos::utility
{

template<class T> struct remove_reference {
    typedef T type;
};
template<class T> struct remove_reference<T &> {
    typedef T type;
};
template<class T> struct remove_reference<T &&> {
    typedef T type;
};

template<class T> typename remove_reference<T>::type &&move(T &&t) noexcept
{
    return static_cast<typename remove_reference<T>::type &&>(t);
}

template< class T >
constexpr T&& forward( remove_reference<T>& t ) noexcept;

template< class T >
constexpr T&& forward( remove_reference<T>&& t ) noexcept;

} // namespace pmos::utility