#pragma once
#include <coroutine>
#include <exception>
#include <pmos/containers/intrusive_list.hh>
#include <pmos/async/coroutines.hh>
#include "port.hh"

struct AHCIPort;

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

pmos::async::detached_task handle_device(AHCIPort &parent);