#pragma once

namespace klib {

template< class T = void >
struct less {
    constexpr bool operator()(const T &lhs, const T &rhs) const 
    {
        return lhs < rhs;
    }
};

}