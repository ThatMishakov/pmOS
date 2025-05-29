#include "ports.h"
#include "system.h"

#include <array>
#include <cerrno>
#include <expected>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pmos
{
enum class RightType {
    SendOnce,
    SendMany,
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

private:
    pmos_right_t right = INVALID_RIGHT;
    RightType type_    = {};
};

inline constexpr Right::Right(Right &&r) noexcept: Right() { *this = std::move(r); }

inline constexpr Right &Right::operator=(Right &&r) noexcept
{
    std::swap(r.right, this->right);
    std::swap(r.type_, this->type_);
    return *this;
}

inline Right::~Right() noexcept
{
    if (*this)
        delete_right(right);
}

inline constexpr Right::operator bool() const noexcept { return right; }

inline constexpr pmos_right_t Right::get() const noexcept { return right; }
inline constexpr pmos_right_t Right::release() noexcept
{
    auto r = right;
    right  = INVALID_RIGHT;
    type_  = {};
    return r;
}

inline constexpr RightType Right::type() const noexcept { return type_; }

inline constexpr Right::Right(pmos_right_t r, RightType t) noexcept: right(r), type_(t) {}

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
    std::expected<std::pair<Right, pmos_right_t>, int> create_right(RightType type) noexcept;

    std::expected<
        std::tuple<Message_Descriptor, std::vector<std::byte>, Right, std::array<Right, 4>>, int>
        get_first_message(bool nonblocking = false);

private:
    pmos_port_t id = 0;
};

inline constexpr Port::Port(pmos_port_t p) noexcept: id(p) {}

inline constexpr Port::Port(Port &&p) noexcept: Port() { *this = std::move(p); }
inline constexpr Port &Port::operator=(Port &&p) noexcept
{
    std::swap(id, p.id);
    return *this;
}

inline constexpr Port::operator bool() const noexcept { return id; }

inline constexpr pmos_port_t Port::get() const noexcept { return id; }

inline constexpr pmos_port_t Port::release() noexcept
{
    auto i = id;
    id     = INVALID_PORT;
    return i;
}

inline Port::~Port() noexcept
{
    if (*this)
        pmos_delete_port(id);
}

inline std::expected<Port, int> Port::create() noexcept
{
    auto result = create_port(TASK_ID_SELF, 0);
    if (result.result)
        return std::unexpected(static_cast<int>(-result.result));

    return Port {result.port};
}

inline std::expected<std::pair<Right, pmos_right_t>, int>
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

    return std::make_pair(Right {r.right, type}, recieve_right);
}

inline template<typename T, typename... Rights>
    requires std::is_trivially_copyable_v<T> && (sizeof...(Rights) <= 4) &&
             (std::is_same_v<std::decay_t<Rights>, Right> && ...)
inline std::expected<Right, int> send_message_right(
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
        flags != SEND_MESSAGE_DELETE_RIGHT;

    auto result = send_message_right(right.get(), rp ? rp->get() : INVALID_PORT,
                                     reinterpret_cast<const void *>(data.data()), data.size_bytes(),
                                     NUM_RIGHTS > 0 ? &extra : nullptr, flags);

    if (result.result)
        return std::unexpected(static_cast<int>(-result.result));

    if (right.type() == RightType::SendOnce || delete_right)
        right.release();

    (rights.release(), ...);

    return Right(result.right, t);
}

inline template<typename T, typename... Rights>
    requires std::is_trivially_copyable_v<T> && (sizeof...(Rights) <= 4) &&
             (std::is_same_v<std::decay_t<Rights>, Right> && ...)
inline std::expected<Right, int> send_message_right_one(
    Right &right, T const &object, std::pair<Port const *, RightType> optional_reply_port,
    bool delete_right = false, Rights... rights) noexcept
{
    return send_message_right<T, Rights...>(right, {&object, 1}, optional_reply_port, delete_right,
                                            std::forward<Rights>(rights)...);
}

inline std::expected<void, int> name_right(Right right, std::string_view name)
{
    auto result = ::name_right(right.get(), name.data(), name.length(), 0);
    if (result)
        return std::unexpected(static_cast<int>(-result));

    right.release();
    return {};
}

inline std::expected<
    std::tuple<Message_Descriptor, std::vector<std::byte>, Right, std::array<Right, 4>>, int>
    Port::get_first_message(bool nonblocking)
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
    data.resize(desc.size);

    if (desc.other_rights_count) {
        std::array<pmos_right_t, 4> rights_ids = {};
        auto result                            = ::accept_rights(id, rights_ids.data());
        if (result)
            return std::unexpected(static_cast<int>(-result));

        for (int i = 0; i < 4; ++i)
            if (rights_ids[i]) {
                bool send_many = desc.flags & (1 << (16 + i));
                rights[i] = {rights_ids[i], send_many ? RightType::SendMany : RightType::SendOnce};
            }
    }

    auto r = ::get_first_message(reinterpret_cast<char *>(data.data()), 0, id);
    if (r.result)
        return std::unexpected(static_cast<int>(-r.result));

    bool send_many = desc.flags & MESSAGE_FLAG_REPLY_SEND_MANY;
    if (r.right)
        reply_right = {r.right, send_many ? RightType::SendMany : RightType::SendOnce};

    return std::make_tuple(desc, std::move(data), std::move(reply_right), std::move(rights));
}

inline std::expected<Right, int> get_right_by_name(std::string_view name, bool noblock = false)
{
    unsigned flags = 0;
    if (noblock)
        flags |= FLAG_NOBLOCK;

    auto result = ::get_right_by_name(name.data(), name.length(), flags);
    if (result.result)
        return std::unexpected(static_cast<int>(-result.result));

    return Right(result.right, RightType::SendMany);
}

}; // namespace pmos