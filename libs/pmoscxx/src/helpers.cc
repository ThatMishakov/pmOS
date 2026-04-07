#include <pmos/helpers.hh>
#include <pmos/memory.h>

namespace pmos
{

static RightType type_from_flags(unsigned flags, int index)
{
    unsigned type = (flags >> (16 + index*4)) & 0xf;

    switch (type) {
    case 1:
        return RightType::SendOnce;
    case 2:
        return RightType::SendMany;
    case 3:
        return RightType::MemObject;
    default:
        return RightType::Unknown;
    }
}

std::expected<Right, int> Right::clone_noexcept() const noexcept
{
    if (!right)
        return std::unexpected(EINVAL);

    if (type_ == RightType::SendOnce)
        return std::unexpected(EINVAL);

    auto req = dup_right(right);
    if (req.result)
        return std::unexpected(-static_cast<int>(req.result));

    return Right {req.right, type_};
}

std::expected<Right, int> create_mem_object_noexcept(uint64_t size, uint32_t flags) noexcept
{
    auto result = ::create_mem_object(size, flags);
    if (result.result)
        return std::unexpected(-static_cast<int>(result.result));

    return Right(result.right, RightType::MemObject);
}

Right Right::clone() const
{
    auto result = clone_noexcept();
    if (not result)
        throw std::system_error(result.error(), std::system_category());

    return std::move(result.value());
}

RecieveRight::~RecieveRight()
{
    if (*this)
        (void)delete_receive_right(port_, right_);
}

PortDispatcher::MessageWaiter::MessageWaiter(pmos::PortDispatcher& d, RecieveRight *right) noexcept:
    recieve_right(right), dispatcher(d)
{
    if (right)
        recieve_right_id = right->get();
    else
        recieve_right_id = 0;
}

PortDispatcher::MessageWaiter PortDispatcher::get_message(RecieveRight &r) noexcept
{
    return MessageWaiter(*this, &r);
}

PortDispatcher::MessageWaiter PortDispatcher::get_message_default() noexcept
{
    return MessageWaiter(*this, nullptr);
}

bool PortDispatcher::MessageWaiter::await_ready() noexcept
{
    return false;
}

void PortDispatcher::MessageWaiter::await_suspend(std::coroutine_handle<> hh)
{
    h = hh;
    if (recieve_right != nullptr) {
        dispatcher.waiters.insert(this);
    } else {
        dispatcher.default_waiter = this;
    }
}

std::expected<void, int> PortDispatcher::dispatch()
{
    while (true) {
        auto msg = port.get_first_message();
        if (!msg)
            return std::unexpected(msg.error());

        MessageWaiter *waiter = NULL;
        auto it = waiters.find(msg->descriptor.sent_with_right);
        if (it != waiters.end()) {
            waiter = &*it;
            waiters.erase(it);
        } else
            waiter = default_waiter;

        if (waiter) {
            waiter->message = std::move(*msg);
            waiter->h.resume();
        } else {
            return std::unexpected(EFAULT);
        }
    }
}

get_msg_return_type PortDispatcher::MessageWaiter::await_resume() noexcept
{
    if (recieve_right and recieve_right->type() == RightType::SendOnce)
        recieve_right->release();

    return std::move(message);
}

get_msg_return_type Port::get_first_message(bool nonblocking)
{
    Message_Descriptor desc;
    std::vector<std::byte> data;
    Right reply_right;
    std::array<Right, 4> rights;

    if (!*this)
        return std::unexpected(EINVAL);

    unsigned flags = 0;
    if (nonblocking)
        flags |= FLAG_NOBLOCK;
    auto result = ::syscall_get_message_info(&desc, id, flags);
    if (result)
        return std::unexpected(static_cast<int>(-result));

    // TODO: Make this not throw
    data.resize(desc.size, std::byte(0xff));

    if (desc.other_rights_count) {
        std::array<pmos_right_t, 4> rights_ids = {};
        auto result                            = ::accept_rights(id, rights_ids.data());
        if (result)
            return std::unexpected(static_cast<int>(-result));

        for (int i = 0; i < 4; ++i)
            if (rights_ids[i]) {
                rights[i] = {rights_ids[i], type_from_flags(desc.flags, i)};
            }
    }

    auto r = ::get_first_message(reinterpret_cast<char *>(data.data()), 0, id);
    if (r.result)
        return std::unexpected(static_cast<int>(-r.result));

    bool send_many = desc.flags & MESSAGE_FLAG_REPLY_SEND_MANY;
    if (r.right)
        reply_right = {r.right, send_many ? RightType::SendMany : RightType::SendOnce};

    return MessageReturn{desc, std::move(data), std::move(reply_right), std::move(rights)};
}

std::expected<Right, int> get_right_by_name(std::string_view name, bool noblock)
{
    unsigned flags = 0;
    if (noblock)
        flags |= FLAG_NOBLOCK;

    auto result = ::get_right_by_name(name.data(), name.length(), flags);
    if (result.result)
        return std::unexpected(static_cast<int>(-result.result));

    return Right(result.right, RightType::SendMany);
}

std::expected<void, int> name_right(Right right, std::string_view name)
{
    auto result = ::name_right(right.get(), name.data(), name.length(), 0);
    if (result)
        return std::unexpected(static_cast<int>(-result));

    right.release();
    return {};
}

std::expected<RecieveRight, int> request_named_port(std::string_view name, Port &port)
{
    auto result = ::request_named_port(name.data(), name.length(), port.get(), 0);
    if (result.result)
        return std::unexpected(-(int)result.result);
    
    return RecieveRight{result.right, RightType::SendOnce, port.get()};
}

std::expected<std::pair<Right, RecieveRight>, int>
    Port::create_right(RightType type) noexcept
{
    if (!*this)
        return std::unexpected(EINVAL);

    unsigned flags = 0;
    if (type == RightType::SendOnce)
        flags |= CREATE_RIGHT_SEND_ONCE;

    pmos_right_t recieve_right;
    auto r = ::create_right(get(), &recieve_right, flags);
    if (r.result)
        return std::unexpected(static_cast<int>(-r.result));

    return std::make_pair(Right {r.right, type}, RecieveRight {recieve_right, type, get()});
}

Port::~Port()
{
    if (*this)
        pmos_delete_port(id);
}

std::expected<Port, int> Port::create() noexcept
{
    auto result = create_port(TASK_ID_SELF, 0);
    if (result.result)
        return std::unexpected(static_cast<int>(-result.result));

    return Port {result.port};
}

Right::~Right()
{
    if (*this)
        delete_right(right);
}


}