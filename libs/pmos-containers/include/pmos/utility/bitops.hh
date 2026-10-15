#pragma once

#include <concepts>

namespace pmos::utility
{

template<typename T>
requires std::is_unsigned_v<T>
constexpr bool is_p2(T x) {
	return x && !(x & (x - 1));
}

} // namespace pmos::utility