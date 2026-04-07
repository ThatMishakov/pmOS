#pragma once
#include <pmos/ports.h>
#include <pmos/system.h>
#include <pmos/containers/intrusive_bst.hh>

#include <array>
#include <cerrno>
#include <expected>
#include <span>
#include <string_view>
#include <utility>
#include <vector>
#include <system_error>
#include <coroutine>

namespace pmos
{
enum class RightType {
    SendOnce,
    SendMany,
    MemObject,
    Unknown,
};

class Right
{
public:
    std::expected<Right, int> duplicate() noexcept;
    constexpr Right() noexcept = default;
    ~Right();

    constexpr Right(Right &&r) noexcept;
    constexpr Right &operator=(Right &&right) noexcept;

    constexpr explicit operator bool() const noexcept;

    constexpr pmos_right_t get() const noexcept;
    constexpr pmos_right_t release() noexcept;

    constexpr RightType type() const noexcept;

    constexpr Right(pmos_right_t right, RightType type) noexcept;

    Right clone() const;
    std::expected<Right, int> clone_noexcept() const noexcept;

private:
    pmos_right_t right = INVALID_RIGHT;
    RightType type_    = {};
};

class RecieveRight
{
public:
    constexpr RecieveRight() noexcept = default;
    constexpr RecieveRight(pmos_right_t right, RightType type, pmos_port_t port) noexcept;
    ~RecieveRight();

    constexpr RecieveRight(RecieveRight &&r) noexcept;
    constexpr RecieveRight &operator=(RecieveRight &&right) noexcept;

    constexpr explicit operator bool() const noexcept;

    constexpr pmos_right_t get() const noexcept;
    constexpr pmos_right_t release() noexcept;

    constexpr pmos_port_t port() const noexcept;

    constexpr RightType type() const noexcept;

private:
    pmos_right_t right_ = INVALID_RIGHT;
    RightType type_    = {};
    pmos_port_t port_ = INVALID_PORT;
};

struct MessageReturn {
    Message_Descriptor descriptor;
    std::vector<std::byte> data;
    Right reply_right;
    std::array<Right, 4> other_rights;
};

using get_msg_return_type = std::expected<MessageReturn, int>;

class Port
{
public:
    constexpr Port() noexcept = default;
    ~Port();

    explicit constexpr Port(pmos_port_t) noexcept;

    constexpr Port(Port &&p) noexcept;
    constexpr Port &operator=(Port &&p) noexcept;

    constexpr explicit operator bool() const noexcept;

    constexpr pmos_port_t get() const noexcept;
    constexpr pmos_port_t release() noexcept;

    static std::expected<Port, int> create() noexcept;
    std::expected<std::pair<Right, RecieveRight>, int> create_right(RightType type) noexcept;

    get_msg_return_type get_first_message(bool nonblocking = false);

private:
    pmos_port_t id = 0;
};

std::expected<RecieveRight, int> request_named_port(std::string_view name, Port &port);

std::expected<Right, int> create_mem_object_noexcept(uint64_t size, uint32_t flags) noexcept;
Right create_mem_object(uint64_t size, uint32_t flags);

template<typename T, typename... Rights>
    requires std::is_trivially_copyable_v<T> && (sizeof...(Rights) <= 4) &&
             (std::is_same_v<std::decay_t<Rights>, Right> && ...)
inline std::expected<RecieveRight, int> send_message_right(
    Right &right, std::span<T const> data, std::pair<Port const *, RightType> optional_reply_port,
    bool delete_right = false, Rights... rights) noexcept
{
    message_extra_t extra = {};

    constexpr std::size_t NUM_RIGHTS = sizeof...(Rights);
    std::size_t idx                  = 0;
    ((extra.extra_rights[idx++] = std::forward<Rights>(rights).get()), ...);

    auto [rp, t] = optional_reply_port;

    int flags {};
    if (rp && t == RightType::SendMany)
        flags |= REPLY_CREATE_SEND_MANY;

    if (delete_right)
        flags |= SEND_MESSAGE_DELETE_RIGHT;

    auto port = rp ? rp->get() : INVALID_PORT;

    auto result = send_message_right(right.get(), port,
                                     reinterpret_cast<const void *>(data.data()), data.size_bytes(),
                                     NUM_RIGHTS > 0 ? &extra : nullptr, flags);

    if (result.result)
        return std::unexpected(static_cast<int>(-result.result));

    if (right.type() == RightType::SendOnce || delete_right)
        right.release();

    (rights.release(), ...);

    return RecieveRight(result.right, t, port);
}

template<typename T, typename... Rights>
    requires std::is_trivially_copyable_v<T> && (sizeof...(Rights) <= 4) &&
             (std::is_same_v<std::decay_t<Rights>, Right> && ...)
inline std::expected<RecieveRight, int> send_message_right_one(
    Right &right, T const &object, std::pair<Port const *, RightType> optional_reply_port,
    bool delete_right = false, Rights... rights) noexcept
{
    return send_message_right<T, Rights...>(right, {&object, 1}, optional_reply_port, delete_right,
                                            std::forward<Rights>(rights)...);
}

class PortDispatcher
{
protected:
    class MessageWaiter {
    public:
        constexpr pmos_right_t get_right_id() const noexcept;

        void await_suspend(std::coroutine_handle<> h);
        bool await_ready() noexcept;
        get_msg_return_type await_resume() noexcept;

    private:
        MessageWaiter(PortDispatcher &dispatcher, RecieveRight *recieve_right) noexcept;

        RecieveRight *recieve_right = nullptr;
        pmos_right_t recieve_right_id;
        pmos::containers::RBTreeNode<MessageWaiter> tree_node = {};
        std::coroutine_handle<> h;
        PortDispatcher &dispatcher;

        get_msg_return_type message;

        friend class PortDispatcher;
    };

    using PortsTree = pmos::containers::RedBlackTree<
        MessageWaiter, &MessageWaiter::tree_node,
        detail::TreeCmp<MessageWaiter, pmos_right_t, &MessageWaiter::recieve_right_id>>;

    Port &port;
    MessageWaiter *default_waiter = {};
    PortsTree::RBTreeHead waiters;
public:

    constexpr PortDispatcher(Port &) noexcept;

    constexpr Port &get_port() noexcept;
    constexpr const Port &get_port() const noexcept;

    MessageWaiter get_message(RecieveRight &) noexcept;
    MessageWaiter get_message_default() noexcept;
    std::expected<void, int> dispatch();
};

std::expected<Right, int> get_right_by_name(std::string_view name, bool noblock = false);
std::expected<void, int> name_right(Right right, std::string_view name);

constexpr Port::Port(pmos_port_t p) noexcept: id(p) {}

constexpr Port::Port(Port &&p) noexcept: Port() { *this = std::move(p); }
constexpr Port &Port::operator=(Port &&p) noexcept
{
    std::swap(id, p.id);
    return *this;
}

constexpr Port::operator bool() const noexcept { return id; }

constexpr pmos_port_t Port::get() const noexcept { return id; }

constexpr pmos_port_t Port::release() noexcept
{
    auto i = id;
    id     = INVALID_PORT;
    return i;
}

constexpr Right::Right(pmos_right_t right, RightType type) noexcept: right(right), type_(type) {}
constexpr Right::Right(Right &&r) noexcept: Right() { *this = std::move(r); }

constexpr Right &Right::operator=(Right &&r) noexcept
{
    std::swap(r.right, this->right);
    std::swap(r.type_, this->type_);
    return *this;
}

constexpr Right::operator bool() const noexcept { return right; }

constexpr pmos_right_t Right::get() const noexcept { return right; }
constexpr pmos_right_t Right::release() noexcept
{
    auto r = right;
    right  = INVALID_RIGHT;
    type_  = {};
    return r;
}

constexpr RightType Right::type() const noexcept { return type_; }

constexpr RecieveRight::RecieveRight(RecieveRight &&r) noexcept: RecieveRight() { *this = std::move(r); }

constexpr RecieveRight &RecieveRight::operator=(RecieveRight &&r) noexcept
{
    std::swap(r.right_, this->right_);
    std::swap(r.type_, this->type_);
    std::swap(r.port_, this->port_);
    return *this;
}

constexpr RecieveRight::operator bool() const noexcept { return right_; }

constexpr pmos_right_t RecieveRight::get() const noexcept { return right_; }
constexpr pmos_right_t RecieveRight::release() noexcept
{
    auto r = right_;
    right_  = INVALID_RIGHT;
    type_  = {};
    port_ = INVALID_PORT;
    return r;
}

constexpr RightType RecieveRight::type() const noexcept { return type_; }

constexpr RecieveRight::RecieveRight(pmos_right_t r, RightType t, pmos_port_t p) noexcept: right_(r), type_(t), port_(p) {}

constexpr Port &PortDispatcher::get_port() noexcept
{
    return port;
}

constexpr PortDispatcher::PortDispatcher(Port &port) noexcept: port(port), default_waiter(nullptr)  {}

}; // namespace pmos