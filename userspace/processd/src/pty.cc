#include "pty.hh"
#include <memory>
#include <pmos/async/coroutines.hh>
#include <pmos/ipc.h>
#include "log.hh"
#include <fcntl.h>

extern pmos::Port main_port;
extern pmos::PortDispatcher dispatcher;

struct PtyData {
    bool have_manager = true;
    bool locked = true;

    unsigned idx = 0;

    std::string name() {
        return "/dev/pts/" + std::to_string(idx);
    }
};

std::expected<std::shared_ptr<PtyData>, int> new_pty()
{
    static unsigned next_idx = 0;
    auto pty = std::make_shared<PtyData>();
    pty->idx = next_idx++;
    return pty;
}

pmos::async::detached_task openpt_manager(pmos::ReceiveRight rr, std::shared_ptr<PtyData> pty, unsigned oflags)
{
    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message(rr)).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posixd: Received very small message while attending pty\n" << frg::endlog;
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            break;
        case IPC_Unlockpt_NUM: {
            if (message.size() < sizeof(IPC_Unlockpt)) {
                kernelLogger() << "posixd: Received IPC_Unlockpt that is too small while attending pty\n" << frg::endlog;
                break;
            }

            pty->locked = false;

            IPC_Unlockpt_Reply reply = {
                .type        = IPC_Unlockpt_Reply_NUM,
                .result_code = 0,
                .flags       = 0,
            };

            auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
            if (!result_send)
                kernelLogger() << "posixd: Error " << result_send.error().first << " sending unlockpt reply to port " << reply_right.get() << "\n" << frg::endlog;
        } break;
        case IPC_Ttyname_NUM: {
            if (message.size() < sizeof(IPC_Ttyname)) {
                kernelLogger() << "posixd: Received IPC_Ttyname that is too small while attending pty\n" << frg::endlog;
                break;
            }

            auto name = pty->name();
            size_t name_length = name.size();

            size_t reply_size = sizeof(IPC_Ttyname_Reply) + name_length;
            std::unique_ptr<char[]> reply_data(new char[reply_size]);
            auto *reply = reinterpret_cast<IPC_Ttyname_Reply *>(reply_data.get());
            reply->type = IPC_Ttyname_Reply_NUM;
            reply->result_code = 0;
            reply->flags = 0;
            memcpy(reply->tty_name, name.c_str(), name_length);

            auto span = std::span<char>(reply_data.get(), reply_size);
            auto result_send = pmos::send_message_right_one(reply_right, span, {}, true);
            if (!result_send)
                kernelLogger() << "posixd: Error " << result_send.error().first << " sending ttyname reply to port " << reply_right.get() << "\n" << frg::endlog;
        }
            break;
        default:
            kernelLogger() << "posixd: Unknown message type " << ipc_msg->type << " while attending pty\n" << frg::endlog;
            break;
        }
    }

    co_return;
}

void openpt_error(pmos::Right &reply_right, int16_t result)
{
    if (!reply_right)
        return;

    IPC_Open_Reply reply = {
        .type        = IPC_Open_Reply_NUM,
        .result_code = result,
        .fs_flags    = 0,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending openpt reply to port " << reply_right.get() << "\n" << frg::endlog;
}

void openpt_handle(pmos::Right reply_right, unsigned oflags)
{
    if (!reply_right)
        return;

    auto pty_e = new_pty();
    if (!pty_e) {
        openpt_error(reply_right, pty_e.error());
        return;
    }
    auto pty = std::move(pty_e.value());

    auto right = main_port.create_right(pmos::RightType::SendMany);
    openpt_manager(std::move(right.value().second), pty, oflags);

    auto op_right = std::move(right.value().first);
    auto io_right = op_right.clone();

    uint16_t flags = O_RDWR | (oflags & O_NONBLOCK);

    IPC_Open_Reply reply = {
        .type        = IPC_Open_Reply_NUM,
        .result_code = 0,
        .fs_flags    = flags,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true, std::move(op_right), std::move(io_right));
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending openpt reply to port " << reply_right.get() << "\n" << frg::endlog;
}