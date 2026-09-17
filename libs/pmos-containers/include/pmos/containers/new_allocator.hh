#pragma once

#include <cstddef>

namespace pmos::containers
{

template<typename T>
struct new_allocator {
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    constexpr new_allocator() noexcept = default;

    template<typename U>
    constexpr new_allocator(const new_allocator<U>&) noexcept {}

    [[nodiscard]]
    constexpr T* allocate(std::size_t n) noexcept
    {
        if (n > static_cast<std::size_t>(-1) / sizeof(T))
            return nullptr;

        return static_cast<T*>(
            ::operator new(n * sizeof(T))
        );
    }

    constexpr void deallocate(T* p, std::size_t) noexcept
    {
        ::operator delete(p);
    }

    template<typename U>
    struct rebind {
        using other = new_allocator<U>;
    };

    constexpr bool operator==(const new_allocator&) const noexcept = default;
};

} // namespace pmos::containers
