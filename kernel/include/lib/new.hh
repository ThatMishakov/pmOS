#pragma once
#include "exception.hh"

namespace std {
    class bad_alloc: exception {
    private:
        constexpr static const char* reason = "std::bad_alloc";
    public:
        bad_alloc() noexcept = default;
		bad_alloc(const bad_alloc&) noexcept = default;
		bad_alloc& operator=(const bad_alloc&) noexcept = default;
		~bad_alloc() noexcept = default;
		virtual const char* what() const noexcept
        {
            return reason;
        }
    };
}