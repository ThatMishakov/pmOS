#pragma once
#include "utility.hh"

namespace pmos::utility
{

template<typename T> class scope_guard
{
public:
    explicit scope_guard(T on_exit_scope): on_exit_scope_(move(on_exit_scope)) {}
    void dismiss() noexcept { dismissed_ = true; }

    ~scope_guard()
    {
        if (!dismissed_)
            on_exit_scope_();
    }

    scope_guard(const scope_guard &)            = delete;
    scope_guard &operator=(const scope_guard &) = delete;

private:
    T on_exit_scope_;
    bool dismissed_ = false;
};

template<typename T> [[nodiscard]] scope_guard<T> make_scope_guard(T on_exit_scope)
{
    return scope_guard(move(on_exit_scope));
}

} // namespace pmos::utility