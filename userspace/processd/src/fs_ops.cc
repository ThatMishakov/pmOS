#include "fs_ops.hh"
#include <pmos/ipc.h>

extern pmos::Port main_port;
extern pmos::PortDispatcher dispatcher;

pmos::async::task<std::expected<void, int>> read_file(pmos::Right &file_right, std::span<uint8_t> buffer, size_t offset)
{
    auto it = buffer.begin();
    auto end = buffer.end();
    while (it != end) {
        IPC_Read request = {
            .type = IPC_Read_NUM,
            .flags = IPC_FLAG_IO_OP_FIXED_OFFSET,
            .start_offset = offset + (uint64_t)(it - buffer.begin()),
            .max_size = (uint64_t)(end - it),
        };

        auto r = pmos::send_message_right_one(file_right, request, {&main_port, pmos::RightType::SendOnce}, false);
        if (!r)
            co_return std::unexpected((int)r.error().first);
        
        auto msg_val = co_await dispatcher.get_message(r.value());
        auto msg = std::move(msg_val.value());
        if (msg.descriptor.size < sizeof(IPC_Read_Reply))
            co_return std::unexpected(-EIO);

        auto *reply = reinterpret_cast<IPC_Read_Reply *>(msg.data.data());
        if (reply->type != IPC_Read_Reply_NUM)
            co_return std::unexpected(-EIO);

        if (reply->result_code < 0)
            co_return std::unexpected(reply->result_code);

        auto data_size = msg.descriptor.size - sizeof(IPC_Read_Reply);
        if (data_size == 0)
            co_return std::unexpected(-EIO);

        auto copy_size = std::min<uint64_t>(data_size, end - it);
        std::copy_n(reply->data, copy_size, it);
        it += copy_size;
    }

    co_return {};
}

pmos::async::task<std::expected<pmos::Right, int>> get_mem_object(pmos::Right &file_right, unsigned permissions)
{
    IPC_Get_Object request = {
        .type = IPC_Get_Object_NUM,
        .flags = permissions,
    };

    auto r = pmos::send_message_right_one(file_right, request, {&main_port, pmos::RightType::SendOnce}, false);
    if (!r)
        co_return std::unexpected((int)r.error().first);

    auto msg_val = co_await dispatcher.get_message(r.value());
    auto msg = std::move(msg_val.value());
    if (msg.descriptor.size < sizeof(IPC_Get_Object_Reply))
        co_return std::unexpected(-EIO);

    auto *reply = reinterpret_cast<IPC_Get_Object_Reply *>(msg.data.data());
    if (reply->type != IPC_Get_Object_Reply_NUM)
        co_return std::unexpected(-EIO);

    if (reply->result_code < 0)
        co_return std::unexpected(reply->result_code);

    if (!msg.other_rights[0])
        co_return std::unexpected(-EIO);

    co_return std::move(msg.other_rights[0]);
}
