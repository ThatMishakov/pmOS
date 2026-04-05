#pragma once
#include <pmos/helpers.hh>
#include <pmos/async/coroutines.hh>
#include <pmos/ipc/bus_object.hh>
#include <pmos/containers/intrusive_list.hh>

namespace pmos {

class PMBUSHelper {
public:
    pmos::async::task<uint64_t> publish_object(pmos::ipc::BUSObject object, Right right);

    PMBUSHelper(PortDispatcher &);
    // I don't know what to do with the list of waiters here...
    // ~PMBUSHelper();
private:
    pmos::async::detached_task request_right();

    struct PMBUSRightWaiter {
        void await_suspend(std::coroutine_handle<> h) noexcept;
        bool await_ready() noexcept;
        Right &await_resume() noexcept;

        PMBUSRightWaiter(PMBUSHelper &h);

        PMBUSHelper &helper;
        std::coroutine_handle<> h_;
        containers::DoubleListHead<PMBUSRightWaiter> ll_head;
    };

    PortDispatcher &dispatcher_;
    Right pmbus_right_;
    bool requested_right_ = false;
    containers::CircularDoubleList<PMBUSRightWaiter, &PMBUSRightWaiter::ll_head> right_waiters;

    void dispatch_waiters();
};

}