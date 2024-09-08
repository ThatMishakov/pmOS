#pragma once

namespace pmos::utility
{

namespace detail
{
    template<class T> struct remove_reference {
        typedef T type;
    };
    template<class T> struct remove_reference<T &> {
        typedef T type;
    };
    template<class T> struct remove_reference<T &&> {
        typedef T type;
    };

    template<class T> typename remove_reference<T>::type &&move(T &&t) noexcept
    {
        return static_cast<typename remove_reference<T>::type &&>(t);
    }
} // namespace detail

template<typename T> class scope_guard
{
public:
    explicit scope_guard(T on_exit_scope): on_exit_scope_(detail::move(on_exit_scope)) {}
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
    return scope_guard(detail::move(on_exit_scope));
}

} // namespace pmos::utility