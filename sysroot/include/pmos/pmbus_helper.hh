#pragma once
#include "helpers.hh"
#include "async/coroutines.hh"
#include "ipc/bus_object.hh"
#include "containers/intrusive_list.hh"

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

// TODO: Move this from headers to a library??

inline bool PMBUSHelper::PMBUSRightWaiter::await_ready() noexcept
{
    return bool{helper.pmbus_right_};
}

inline void PMBUSHelper::PMBUSRightWaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    h_ = h;

    helper.right_waiters.push_back(this);

    if (!helper.requested_right_)
        helper.request_right();
}

inline Right &PMBUSHelper::PMBUSRightWaiter::await_resume() noexcept
{
    return helper.pmbus_right_;
}

inline PMBUSHelper::PMBUSRightWaiter::PMBUSRightWaiter(PMBUSHelper &h): helper(h) {}

inline PMBUSHelper::PMBUSHelper(PortDispatcher &d): dispatcher_(d), pmbus_right_({}), requested_right_(false) {}

inline void PMBUSHelper::dispatch_waiters()
{
    auto it = right_waiters.begin();
    while (it != right_waiters.end()) {
        right_waiters.remove(it);
        it->h_.resume();
        it = right_waiters.begin();
    }
}

inline pmos::async::detached_task PMBUSHelper::request_right()
{
    // TODO: This is kinda bad, in case the parent is destroyed, while this is running...
    auto right = request_named_port("/pmos/pmbus", dispatcher_.get_port());
    if (!right)
        throw std::system_error(right.error(), std::system_category());
    
    requested_right_ = true;

    // TODO!
    auto msg = co_await dispatcher_.get_message(right.value());
    if (!msg) {
        requested_right_ = false;
        throw std::system_error(msg.error(), std::system_category());
    }

    pmbus_right_ = std::move(msg->other_rights[0]);

    requested_right_ = false;

    dispatch_waiters();

    co_return;
}

inline pmos::async::task<uint64_t> PMBUSHelper::publish_object(pmos::ipc::BUSObject object, Right right)
{
    auto &pmbus_right = co_await PMBUSRightWaiter{*this};

    auto vec = object.serialize_into_ipc();
    auto span = std::span<const uint8_t>(vec);
    auto result = send_message_right(pmbus_right, span, std::pair{&dispatcher_.get_port(), RightType::SendOnce}, false, std::move(right));
    if (!result)
        throw std::system_error(result.error(), std::system_category());
    
    auto msg = co_await dispatcher_.get_message(result.value());
    if (!msg)
        throw std::system_error(msg.error(), std::system_category());

    if (msg->descriptor.size < sizeof(IPC_BUS_Publish_Object_Reply))
        throw std::system_error(EINTR, std::system_category());

    auto *reply = reinterpret_cast<IPC_BUS_Publish_Object_Reply *>(msg->data.data());
    if (reply->type != IPC_BUS_Publish_Object_Reply_NUM)
        throw std::system_error(EINTR, std::system_category());
        
    if (reply->result)
        throw std::system_error(-reply->result, std::system_category());

    co_return reply->sequence_number;
}

}