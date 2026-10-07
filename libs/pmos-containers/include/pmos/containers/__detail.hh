#pragma once
#include <utility>

namespace pmos::containers::detail {

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

} // namespace pmos::containers::detail