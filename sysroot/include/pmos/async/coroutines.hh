#pragma once
#include <coroutine>
#include <utility>
#include <exception>

namespace pmos::async
{

template<typename T> class task
{
public:
    struct promise_type {
        struct final_awaiter {
            constexpr bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h)
            {
                return h.promise().continuation;
            }
            void await_resume() noexcept {}
        };

        std::coroutine_handle<> continuation = std::noop_coroutine();
        T value;

        task get_return_object()
        {
            return task(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() { return {}; }
        final_awaiter final_suspend() noexcept { return {}; }
        void unhandled_exception() { std::terminate(); }
        final_awaiter yield_value(T v)
        {
            value = std::move(v);
            return {};
        }

        std::suspend_never return_value(T v) noexcept
        {
            value = std::move(v);
            return {};
        }
    };

    operator std::coroutine_handle<promise_type>() const noexcept { return h_; }

    class awaiter
    {
    public:
        constexpr bool await_ready() const noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h)
        {
            h_.promise().continuation = h;
            return h_;
        }
        T && await_resume() {return std::move(h_.promise().value);}

    private:
        std::coroutine_handle<promise_type> h_;
        explicit awaiter(std::coroutine_handle<promise_type> h): h_(h) {}
        friend task<T>;
    };

    awaiter operator co_await() && noexcept { return awaiter (h_); }

    ~task()
    {
        if (h_)
            h_.destroy();
    }

private:
    std::coroutine_handle<promise_type> h_;
    explicit task(std::coroutine_handle<promise_type> h): h_(h) {}
};

class detached_task
{
public:
    struct promise_type {
        detached_task get_return_object()
        {
            return detached_task(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

private:
    std::coroutine_handle<promise_type> h_;
    explicit detached_task(std::coroutine_handle<promise_type> h): h_(h) {}
};

} // namespace pmos::async