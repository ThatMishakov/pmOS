#pragma once

namespace klib {

template<class T, class... Args>
constexpr T* construct_at( T* p, Args&&... args );

}