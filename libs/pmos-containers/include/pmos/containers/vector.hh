#pragma once
#include <cstddef>
#include <iterator>
#include <memory>
#include <ranges>
#include "new_allocator.hh"

namespace pmos::containers
{

namespace detail {

template<typename R, typename T>
concept container_compatible_range =
    std::ranges::input_range<R> &&
    std::convertible_to<std::ranges::range_reference_t<R>, T>;

template<class T>
constexpr auto synth_three_way(const T& lhs, const T& rhs)
{
    if constexpr (requires { lhs <=> rhs; }) {
        return lhs <=> rhs;
    } else {
        if (lhs < rhs)
            return std::strong_ordering::less;
        if (rhs < lhs)
            return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
}

template<class T>
using synth_three_way_result =
    decltype(synth_three_way(
        std::declval<const T&>(),
        std::declval<const T&>()));

}

template<typename T, typename Allocator = new_allocator<T>> class vector
{
public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = std::allocator_traits<Allocator>::pointer;
    using const_pointer = std::allocator_traits<Allocator>::const_pointer;
    using iterator = T*;
    using const_iterator = const T*;

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    constexpr vector() noexcept;
    constexpr explicit vector(const Allocator& alloc) noexcept;
    constexpr vector(vector<T, Allocator> &&) noexcept;
    constexpr ~vector() noexcept;

    constexpr vector<T, Allocator> &operator=(vector<T, Allocator> &&from) noexcept;
    
    constexpr bool assign(size_t count, const T &value) noexcept;
    template<class InputIt>
    constexpr bool assign(InputIt first, InputIt last) noexcept;
    constexpr bool assign(std::initializer_list<T> ilist) noexcept;

    constexpr allocator_type get_allocator() const noexcept;

    constexpr pointer at(size_type pos) noexcept;
    constexpr const_pointer at(size_type pos) const noexcept;

    constexpr T *get(size_type pos) noexcept;
    constexpr const T *get(size_type pos) const noexcept;

    constexpr reference operator[](size_type pos) noexcept;
    constexpr const_reference operator[](size_type pos) const noexcept;

    constexpr reference front() noexcept;
    constexpr const_reference front() const noexcept;

    constexpr reference back() noexcept;
    constexpr const_reference back() const noexcept;

    constexpr T *data() noexcept;
    constexpr const T *data() const noexcept;

    constexpr iterator begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr const_iterator cbegin() const noexcept;

    constexpr iterator end() noexcept;
    constexpr const_iterator end() const noexcept;
    constexpr const_iterator cend() const noexcept;

    constexpr reverse_iterator rbegin() noexcept;
    constexpr const_reverse_iterator rbegin() const noexcept;
    constexpr const_reverse_iterator crbegin() const noexcept;
    constexpr reverse_iterator rend() noexcept;
    constexpr const_reverse_iterator rend() const noexcept;
    constexpr const_reverse_iterator crend() const noexcept;

    constexpr bool empty() const noexcept;
    constexpr size_type size() const noexcept;

    // size_type max_size() const noexcept;
    constexpr bool reserve(size_type new_cap) noexcept;
    constexpr size_type capacity() const noexcept;

    constexpr void shrink_to_fit() noexcept;


    constexpr void clear() noexcept;

    constexpr iterator insert(const_iterator pos, const T& value) noexcept;
    constexpr iterator insert(const_iterator pos, T&& value) noexcept;
    constexpr iterator insert(const_iterator pos, size_type count, const T& value) noexcept;
    template<class InputIt>
    constexpr iterator insert(const_iterator pos, InputIt first, InputIt last) noexcept;
    constexpr iterator insert(const_iterator pos, std::initializer_list<T> ilist) noexcept;

    template<detail::container_compatible_range<T> R>
    constexpr iterator insert_range(const_iterator pos, R&& range) noexcept;

    template<class... Args>
    constexpr iterator emplace(const_iterator pos, Args&&... args) noexcept;

    constexpr iterator erase(const_iterator pos) noexcept;
    constexpr iterator erase(const_iterator first, const_iterator last) noexcept;

    constexpr bool push_back(const T &value) noexcept;
    constexpr bool push_back(T &&value) noexcept;
    template<class... Args>
    constexpr bool emplace_back(Args&&... args) noexcept;
    template<detail::container_compatible_range<T> R>
    constexpr bool append_range(R&& range) noexcept;

    constexpr void pop_back() noexcept;

    constexpr bool resize(size_type n) noexcept;
    constexpr bool resize(size_type n, const value_type &val) noexcept;

    constexpr void swap(vector& other) noexcept;
private:
    static const size_t start_size = 16;

    T *storage_ = nullptr;
    size_t capacity_ = 0;
    size_t size_ = 0;
    allocator_type allocator_{};

    constexpr bool expand(size_t to);

    constexpr bool add_capacity(size_t new_elements) noexcept;
};

template<class T, class Alloc>
constexpr detail::synth_three_way_result<T>
operator<=>(const vector<T, Alloc>& lhs,
            const vector<T, Alloc>& rhs);




template<class T, class Allocator>
constexpr vector<T, Allocator>::vector():
    vector(Allocator())
{}

template<class T, class Allocator>
constexpr vector<T, Allocator>::vector(const Allocator& alloc) noexcept:
    allocator_(alloc)
{}

template<class T, class Allocator>
constexpr vector<T, Allocator>::vector(vector<T, Allocator> &&from) noexcept:
    storage_(from.storage_),
    capacity_(from.capacity_),
    size_(from.size_),
    allocator_(std::move(from.allocator_))
{
    from.storage_  = nullptr;
    from.capacity_ = 0;
    from.size_     = 0;
}

template<class T, class Allocator>
constexpr vector<T, Allocator> &vector<T, Allocator>::operator=(vector<T, Allocator> &&from) noexcept
{
    if (this != &from) {
        clear();
        allocator_.deallocate(storage_, capacity_);
        storage_  = from.storage_;
        capacity_ = from.capacity_;
        size_     = from.size_;
        allocator_ = std::move(from.allocator_);

        from.storage_  = nullptr;
        from.capacity_ = 0;
        from.size_     = 0;
    }
    return *this;
}

template<class T, class Allocator>
constexpr vector<T, Allocator>::~vector() noexcept
{
    clear();
    allocator_.deallocate(storage_, capacity_);
}

template<class T, class Allocator>
constexpr T &vector<T, Allocator>::front() noexcept
{
    return *storage_;
}
template<class T, class Allocator>
constexpr const T &vector<T, Allocator>::front() const noexcept
{
    return *storage_;
}

template<class T, class Allocator>
constexpr T *vector<T, Allocator>::get(size_type pos) noexcept
{
    if (pos >= size_)
        return nullptr;
    return storage_ + pos;
}
template<class T, class Allocator>
constexpr const T *vector<T, Allocator>::get(size_type pos) const noexcept
{
    if (pos >= size_)
        return nullptr;
    return storage_ + pos;
}

template<class T, class Allocator>
constexpr T &vector<T, Allocator>::operator[](size_type pos) noexcept
{
    return storage_[pos];
}
template<class T, class Allocator>
constexpr const T &vector<T, Allocator>::operator[](size_type pos) const noexcept
{
    return storage_[pos];
}

template<class T, class Allocator>
constexpr vector<T, Allocator>::iterator vector<T, Allocator>::begin() noexcept
{
    return storage_;
}

template<class T, class Allocator>
constexpr vector<T, Allocator>::const_iterator vector<T, Allocator>::begin() const noexcept
{
    return storage_;
}

template<class T, class Allocator>
constexpr vector<T, Allocator>::const_iterator vector<T, Allocator>::cbegin() const noexcept
{
    return storage_;
}

template<class T, class Allocator>
constexpr vector<T, Allocator>::iterator vector<T, Allocator>::end() noexcept
{
    return storage_ + size_;
}

template<class T, class Allocator>
constexpr vector<T, Allocator>::const_iterator vector<T, Allocator>::end() const noexcept
{
    return storage_ + size_;
}

template<class T, class Allocator>
constexpr vector<T, Allocator>::const_iterator vector<T, Allocator>::cend() const noexcept
{
    return storage_ + size_;
}

template<class T, class Allocator>
constexpr bool vector<T, Allocator>::empty() const noexcept
{
    return size_ == 0;
}

template<class T, class Allocator>
constexpr size_t vector<T, Allocator>::size() const noexcept
{
    return size_;
}

template<class T, class Allocator>
constexpr size_t vector<T, Allocator>::capacity() const noexcept
{
    return capacity_;
}

template<class T, class Allocator>
constexpr T *vector<T, Allocator>::data() noexcept
{
    return storage_;
}

template<class T, class Allocator>
constexpr const T *vector<T, Allocator>::data() const noexcept
{
    return storage_;
}

template<class T, class Allocator>
constexpr void vector<T, Allocator>::clear() noexcept
{
    for (size_t i = 0; i < size_; ++i)
        storage_[i].~T();

    size_ = 0;
}

template<class T, class Allocator>
constexpr bool vector<T, Allocator>::reserve(size_type new_cap) noexcept
{
    if (new_cap <= capacity_)
        return true;

    return expand(new_cap);
}

template<class T, class Allocator>
constexpr bool vector<T, Allocator>::resize(size_type n) noexcept
{
    if (n < size_) {
        for (size_t i = n; i < size_; ++i)
            storage_[i].~T();
    } else if (n > size_) {
        if (!reserve(n))
            return false;

        for (size_t i = size_; i < n; ++i)
            new (&storage_[i]) T();
    }

    size_ = n;
    return true;
}

template<class T, class Allocator>
constexpr bool vector<T, Allocator>::resize(size_type n, const T &value) noexcept
{
    if (n < size_) {
        for (size_t i = n; i < size_; ++i)
            storage_[i].~T();
    } else if (n > size_) {
        if (!reserve(n))
            return false;

        for (size_t i = size_; i < n; ++i)
            new (&storage_[i]) T(value);
    }

    size_ = n;
    return true;
}

template<class T, class Allocator>
constexpr typename vector<T, Allocator>::iterator
vector<T, Allocator>::insert(const_iterator pos, size_type count, const T &value) noexcept
{
    size_t idx = static_cast<size_t>(pos - storage_);
    size_t current_size = size_;

    if (!add_capacity(count))
        return end();

    for (size_t i = current_size; i > idx; --i)
        new (&storage_[i + count - 1]) T(std::move(storage_[i - 1]));

    size_t up_to = idx + count;
    for (size_t i = idx; i < up_to; ++i)
        storage_[i] = value;

    size_ = current_size + count;
    return storage_ + idx;
}

template<class T, class Allocator>
constexpr bool vector<T, Allocator>::add_capacity(size_t new_elements) noexcept
{
    size_t required = size_ + new_elements;
    if (required <= capacity_)
        return true;

    size_t cap = capacity_ == 0 ? start_size : capacity_;
    while (cap < required)
        cap *= 2;

    return expand(cap);
}

template<class T, class Allocator>
constexpr bool vector<T, Allocator>::push_back(const T &p) noexcept
{
    if (!add_capacity(1))
        return false;

    new (&storage_[size_++]) T(p);
    return true;
}

template<class T, class Allocator>
constexpr bool vector<T, Allocator>::push_back(T &&p) noexcept
{
    if (!add_capacity(1))
        return false;

    new (&storage_[size_++]) T(std::move(p));
    return true;
}

template<class T, class Allocator>
template<detail::container_compatible_range<T> R>
constexpr bool vector<T, Allocator>::append_range(R&& rg) noexcept
{
    if constexpr (std::ranges::sized_range<R>) {
        const auto size = std::ranges::size(rg);
        if (!add_capacity(size))
            return false;
    }

    auto old_size = size_;
    bool success = true;
    for (auto &&item : rg) {
        if (!push_back(std::forward<decltype(item)>(item))) {
            success = false;
            break;
        }
    }

    if (!success)
        (void)resize(old_size);

    return success;
}

template<class T, class Allocator>
constexpr bool vector<T, Allocator>::expand(size_t new_capacity)
{
    if (new_capacity <= capacity_)
        return true;

    size_t new_cap = capacity_ == 0 ? start_size : capacity_;
    auto ptr = allocator_.allocate(new_cap);
    if (!ptr)
        return false;

    for (size_t i = 0; i < size_; ++i)
        new (&ptr[i]) T(std::move(storage_[i]));

    for (size_t i = 0; i < size_; ++i)
        storage_[i].~T();

    allocator_.deallocate(storage_, capacity_);
    storage_ = ptr;
    capacity_ = new_cap;
    return true;
}

} // namespace pmos::containers