#pragma once
#include "vector.hh"
#include "__detail.hh"
#include <string_view>
#include "new_allocator.hh"
#include <iterator>

namespace pmos::containers
{

template<typename CharT, typename Allocator = new_allocator<CharT>>
class basic_string
{
public:
    using value_type = CharT;
    using allocator_type = Allocator;
    using size_type = size_t;
    using pointer = CharT*;
    using const_pointer = const CharT*;
    using iterator = CharT*;
    using const_iterator = const CharT*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type npos = static_cast<size_type>(-1);

    constexpr basic_string() noexcept;
    constexpr basic_string(const basic_string &other) noexcept = delete;
    constexpr basic_string(basic_string &&other) noexcept;
    constexpr ~basic_string() noexcept;
    constexpr basic_string &operator=(const basic_string &other) noexcept = delete;
    constexpr basic_string &operator=(basic_string &&other) noexcept;

    constexpr bool assign(const basic_string &other) noexcept;
    constexpr bool assign(basic_string &&other) noexcept;
    constexpr bool assign(size_type count, CharT c) noexcept;
    constexpr bool assign(const CharT *str, size_type len) noexcept;
    constexpr bool assign(const CharT *str) noexcept;
    template<typename SV>
    constexpr bool assign(const SV &str) noexcept;
    template<typename SV>
    constexpr bool assign(const SV &str, size_type pos, size_type count = npos) noexcept;
    constexpr bool assign(const basic_string &other, size_type pos, size_type count = npos) noexcept;
    template<class InputIt>
    constexpr bool assign(InputIt first, InputIt last) noexcept;
    constexpr bool assign(std::initializer_list<CharT> ilist) noexcept;

    template<detail::container_compatible_range<CharT> R>
    constexpr bool assign_range(R&& range) noexcept;

    constexpr CharT& operator[](size_type pos) noexcept;
    constexpr const CharT& operator[](size_type pos) const noexcept;

    constexpr CharT& front() noexcept;
    constexpr const CharT& front() const noexcept;
    constexpr CharT& back() noexcept;
    constexpr const CharT& back() const noexcept;

    constexpr CharT* data() noexcept;
    constexpr const CharT* data() const noexcept;
    constexpr const CharT* c_str() const noexcept;

    constexpr operator std::basic_string_view<CharT>() const noexcept;
    
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
    constexpr size_type length() const noexcept;
    bool reserve(size_type capacity) noexcept;
    constexpr size_type capacity() const noexcept;
    constexpr void shrink_to_fit() noexcept;

    constexpr void clear() noexcept;

    constexpr bool insert(size_type index, size_type count, CharT c) noexcept;
    constexpr bool insert(size_type index, const CharT *str) noexcept;
    constexpr bool insert(size_type index, const CharT *str, size_type count) noexcept;
    constexpr bool insert(size_type index, const basic_string &str) noexcept;
    constexpr bool insert(size_type index, const basic_string &str, size_type index_str, size_type count = npos) noexcept;

    // end() -> allocation failure
    constexpr iterator insert(const_iterator pos, CharT c) noexcept;
    constexpr iterator insert(const_iterator pos, size_type count, CharT c) noexcept;
    template<class InputIt>
    constexpr iterator insert(const_iterator pos, InputIt first, InputIt last) noexcept;
    constexpr iterator insert(const_iterator pos, std::initializer_list<CharT> ilist) noexcept;
    template<class SV>
    constexpr iterator insert(const_iterator pos, const SV &str) noexcept;
    template<class SV>
    constexpr iterator insert(const_iterator pos, const SV &str, size_type index_str, size_type count = npos) noexcept;

    template<detail::container_compatible_range<CharT> R>
    constexpr iterator insert_range(const_iterator pos, R&& range) noexcept;

    constexpr basic_string &erase(size_type index = 0, size_type count = npos) noexcept;
    constexpr iterator erase(const_iterator pos) noexcept;
    constexpr iterator erase(const_iterator first, const_iterator last) noexcept;

    constexpr bool push_back(CharT c) noexcept;
    constexpr void pop_back() noexcept;

    constexpr bool append(size_type count, CharT c) noexcept;
    constexpr bool append(const CharT *str) noexcept;
    constexpr bool append(const CharT *str, size_type count) noexcept;
    template<class SV>
    constexpr bool append(const SV &str) noexcept;
    template<class SV>
    constexpr bool append(const SV &str, size_type index_str, size_type count = npos) noexcept;
    constexpr bool append(const basic_string &str) noexcept;
    constexpr bool append(const basic_string &str, size_type index_str, size_type count = npos) noexcept;
    template<class InputIt>
    constexpr bool append(InputIt first, InputIt last) noexcept;
    constexpr bool append(std::initializer_list<CharT> ilist) noexcept;
    
    template<detail::container_compatible_range<CharT> R>
    constexpr bool append_range(R&& range) noexcept;

    constexpr bool operator+=(const basic_string &str) noexcept;
    constexpr bool operator+=(CharT c) noexcept;
    constexpr bool operator+=(const CharT *str) noexcept;
    constexpr bool operator+=(std::initializer_list<CharT> ilist) noexcept;
    template<class SV>
    constexpr bool operator+=(const SV &str) noexcept;

    // TODO: replace, copy

    constexpr bool resize(size_type new_size, CharT c = CharT{}) noexcept;
    template<class Operation>
    constexpr bool resize_and_overwrite(size_type new_size, Operation op) noexcept;

    constexpr void swap(basic_string &other) noexcept;

    // TODO: find, compare, starts_with, ends_with, contains, substr, subview


private:
    size_t capacity_ = 0;

    struct LongString {
        size_t size = 0;
        CharT *data = nullptr;
    };

    static constexpr size_t short_string_capacity_ = sizeof(LongString) / sizeof(CharT) - 1;

    union {
        LongString long_string_;
        CharT short_string_[sizeof(LongString) / sizeof(CharT)];
    };

    allocator_type allocator_;

    constexpr bool is_long() const noexcept;

    constexpr bool ensure_capacity(size_type new_capacity) noexcept;
    constexpr void set_size(size_type new_size) noexcept;
};

using string = basic_string<char>;

template<typename CharT, typename Allocator>
constexpr size_t basic_string<CharT, Allocator>::size() const noexcept
{
    return is_long() ? long_string_.size : capacity_;
}
template<typename CharT, typename Allocator>
constexpr size_t basic_string<CharT, Allocator>::length() const noexcept
{
    return size();
}
template<typename CharT, typename Allocator>
constexpr basic_string<CharT, Allocator>::operator std::basic_string_view<CharT>() const noexcept
{
    return std::basic_string_view<CharT>(data(), size());
}
template<typename CharT, typename Allocator>
constexpr CharT* basic_string<CharT, Allocator>::data() noexcept
{
    return is_long() ? long_string_.data : short_string_;
}
template<typename CharT, typename Allocator>
constexpr const CharT* basic_string<CharT, Allocator>::data() const noexcept
{
    return is_long() ? long_string_.data : short_string_;
}
template<typename CharT, typename Allocator>
constexpr const CharT* basic_string<CharT, Allocator>::c_str() const noexcept
{
    return data();
}
template<typename CharT, typename Allocator>
constexpr CharT &basic_string<CharT, Allocator>::operator[](size_type pos) noexcept
{
    return *(data() + pos);
}
template<typename CharT, typename Allocator>
constexpr bool basic_string<CharT, Allocator>::is_long() const noexcept
{
    return capacity_ > short_string_capacity_;
}

template<typename CharT, typename Allocator>
constexpr void basic_string<CharT, Allocator>::set_size(size_type new_size) noexcept
{
    if (is_long())
        long_string_.size = new_size;
    else
        capacity_ = new_size;
}

template<typename CharT, typename Allocator>
constexpr basic_string<CharT, Allocator>::basic_string() noexcept
    : capacity_(0), long_string_({0, nullptr})
{}
template<typename CharT, typename Allocator>
constexpr basic_string<CharT, Allocator>::basic_string(basic_string &&other) noexcept
    : capacity_(other.capacity_), long_string_(other.long_string_)
{
    other.capacity_ = 0;
    other.long_string_ = {0, nullptr};
}
template<typename CharT, typename Allocator>
constexpr basic_string<CharT, Allocator>::~basic_string() noexcept
{
    if (is_long()) {
        allocator_.deallocate(long_string_.data, long_string_.size);
    }
}

template<typename CharT, typename Allocator>
template<typename SV>
constexpr bool basic_string<CharT, Allocator>::assign(const SV &str) noexcept
{
    return assign(str.data(), str.size());
}

template<typename CharT, typename Allocator>
constexpr bool basic_string<CharT, Allocator>::assign(const CharT *str, size_type len) noexcept
{
    return assign(str, str + len);
}

template<typename CharT, typename Allocator>
template<typename Iterator>
constexpr bool basic_string<CharT, Allocator>::assign(Iterator first, Iterator last) noexcept
{
    auto distance = std::distance(first, last);
    if (distance < 0)
        return false;

    if (!ensure_capacity(static_cast<size_type>(distance)))
        return false;

    size_type idx = 0;
    for (auto it = first; it != last; ++it) {
        (*this)[idx++] = *it;
    }
    (*this)[idx] = CharT{};
    set_size(static_cast<size_type>(distance));
    return true;
}

template<typename CharT, typename Allocator>
constexpr bool basic_string<CharT, Allocator>::ensure_capacity(size_type new_capacity) noexcept
{
    if (is_long()) {
        if (new_capacity <= capacity_)
            return true;

        auto new_data = allocator_.allocate(new_capacity + 1);
        if (!new_data)
            return false;

        for (size_type i = 0; i < long_string_.size; ++i) {
            new_data[i] = long_string_.data[i];
        }
        new_data[long_string_.size] = CharT{};

        allocator_.deallocate(long_string_.data, long_string_.size);
        long_string_.data = new_data;
        capacity_ = new_capacity;
    } else {
        if (new_capacity <= short_string_capacity_)
            return true;

        auto new_data = allocator_.allocate(new_capacity + 1);
        if (!new_data)
            return false;

        for (size_type i = 0; i < capacity_; ++i) {
            new_data[i] = short_string_[i];
        }
        new_data[capacity_] = CharT{};

        long_string_.data = new_data;
        long_string_.size = capacity_;
        capacity_ = new_capacity;
    }
    return true;
}

} // namespace pmos::containers