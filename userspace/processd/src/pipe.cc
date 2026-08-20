#include "pipe.hh"
#include "log.hh"
#include <thread>

#include <pmos/async/coroutines.hh>

thread_local bool have_reader = true;
thread_local bool have_writer = true;

pmos::async::detached_task get_messages_pipe_reader(pmos::PortDispatcher &dispatcher, pmos::RecieveRight rr)
{
    while (1) {
        auto [msg, message, reply_right, _] = (co_await dispatcher.get_message(rr)).value();
    
        if (msg.size < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "processd: Recieved very small message\n" << frg::endlog;
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Recieve_Right_Destroyed_NUM:
            co_return;

        default:
            kernelLogger() << "processd: Unknown message type " << ipc_msg->type << " from pipe reader\n" << frg::endlog;
            break;
        }
    }

    have_reader = false;
}

pmos::async::detached_task get_messages_pipe_writer(pmos::PortDispatcher &dispatcher, pmos::RecieveRight rr)
{
    while (1) {
        auto [msg, message, reply_right, _] = (co_await dispatcher.get_message(rr)).value();
    
        if (msg.size < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "processd: Recieved very small message\n" << frg::endlog;
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Recieve_Right_Destroyed_NUM:
            co_return;

        default:
            kernelLogger() << "processd: Unknown message type " << ipc_msg->type << " from pipe writer\n" << frg::endlog;
            break;
        }
    }

    have_writer = false;
}

void pipe_thread(IPC_Pipe_Open msg, pmos::Right reply_right)
{
    auto port = pmos::Port::create().value();
    pmos::PortDispatcher dispatcher(port);

    auto sr = port.create_right(pmos::RightType::SendMany).value();
    auto rr = port.create_right(pmos::RightType::SendMany).value();

    auto [r, read_right] = std::move(sr);
    auto [r2, write_right] = std::move(rr);

    IPC_Pipe_Open_Reply reply = {
        .type        = IPC_Pipe_Open_Reply_NUM,
        .flags       = 0,
        .result_code = 0,
    };

    auto send_result = pmos::send_message_right_one(reply_right, reply, {}, true, std::move(r), std::move(r2));
    if (!send_result) {
        kernelLogger() << "processd: Error " << send_result.error() << " sending message to port " << reply_right.get() << " for pipe_open\n" << frg::endlog;
        return;
    }

    get_messages_pipe_reader(dispatcher, std::move(read_right));
    get_messages_pipe_writer(dispatcher, std::move(write_right));

    dispatcher.dispatch();
}

void pipe_open(IPC_Pipe_Open &msg, pmos::Right reply_right)
{
    kernelLogger() << "processd: Creating pipe\n" << frg::endlog;

    auto thread = std::thread(pipe_thread, msg, std::move(reply_right));
    thread.detach();
}