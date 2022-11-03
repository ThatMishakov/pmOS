#pragma once
#include <stddef.h>
#include <stdint.h>

namespace klib {

class string {
private:
    size_t capacity;

    union Str {
        char str[16];
        struct long_s
        {
            char* ptr;
            uint64_t length;
        };
    } s;
public:
    constexpr string(): capacity(0), s({}) {};
};

}