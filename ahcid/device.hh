#pragma once
#include <coroutine>
#include <exception>
#include <pmos/containers/intrusive_list.hh>
#include "port.hh"

struct AHCIPort;

struct AHCIDeviceReturnObject {
    struct promise_type {
        AHCIDeviceReturnObject get_return_object() { return AHCIDeviceReturnObject {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> h_;
    operator std::coroutine_handle<promise_type>() const { return h_; }
};

struct GetCmdIndex : CmdPortWaiter {
    AHCIPort &parent;
    std::coroutine_handle<> h_;
    static void available_callback(CmdPortWaiter *self);

    GetCmdIndex(AHCIPort &p): parent(p) {
        wait_callback = available_callback;
    }

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> h);
    int await_resume();
};

AHCIDeviceReturnObject handle_device(AHCIPort &parent);