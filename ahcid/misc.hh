#pragma once

template <typename T>
class scope_guard {
public:
    explicit scope_guard(T on_exit_scope)
        : on_exit_scope_(std::move(on_exit_scope))
    {
    }

    ~scope_guard() { on_exit_scope_(); }

    scope_guard(const scope_guard &) = delete;
    scope_guard &operator=(const scope_guard &) = delete;
private:
    T on_exit_scope_;
};

template <typename T>
[[nodiscard]] scope_guard<T> make_scope_guard(T on_exit_scope)
{
    return scope_guard(std::move(on_exit_scope));
}