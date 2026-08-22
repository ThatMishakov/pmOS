#include "pipe.hh"
#include "log.hh"
#include <thread>
#include <memory>
#include <limits.h>
#include <deque>

#include <pmos/async/coroutines.hh>

#include <frg/ringbuffer.hpp>
#include <frg/std_compat.hpp>

struct PendingWrite {
    pmos::Right reply_right;
    frg::span<const uint8_t> pending_data;
    std::vector<std::byte> message_data;
    bool noblock;
    size_t bytes_written;
};

struct PipeData {
    bool have_reader = true;
    bool have_writer = true;
    frg::byte_ring_buffer<frg::stl_allocator> buffer = frg::byte_ring_buffer<frg::stl_allocator>(PIPE_BUF);

    std::deque<PendingWrite> pending_writes;
};

static void write_reply(pmos::Right &reply_right, int result_code, size_t bytes_written)
{
    IPC_Write_Reply reply = {
        .type        = IPC_Write_Reply_NUM,
        .flags       = 0,
        .result_code = static_cast<int16_t>(result_code),
        .bytes_written = bytes_written,
    };

    auto send_result = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!send_result) {
        kernelLogger() << "processd: Error " << send_result.error() << " sending message to port " << reply_right.get() << " for pipe_write\n" << frg::endlog;
    }
}

static void epipe_writers(PipeData &pipe_data)
{
    while (!pipe_data.pending_writes.empty()) {
        auto &pending_writer = pipe_data.pending_writes.front();
        if (pending_writer.bytes_written == 0) {
            write_reply(pending_writer.reply_right, -EPIPE, 0);
        } else {
            write_reply(pending_writer.reply_right, 0, pending_writer.bytes_written);
        }
        pipe_data.pending_writes.pop_front();
    }
}

pmos::async::detached_task get_messages_pipe_reader(pmos::PortDispatcher &dispatcher, PipeData &pipe_data, pmos::RecieveRight rr)
{
    while (pipe_data.have_reader) {
        auto [msg, message, reply_right, _] = (co_await dispatcher.get_message(rr)).value();
    
        if (msg.size < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "processd: Recieved very small message\n" << frg::endlog;
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Recieve_Right_Destroyed_NUM:
            pipe_data.have_reader = false;
            break;

        default:
            kernelLogger() << "processd: Unknown message type " << ipc_msg->type << " from pipe reader\n" << frg::endlog;
            break;
        }
    }

    epipe_writers(pipe_data);
}

static void wakeup_readers(PipeData &pipe_data)
{
    // TODO...
}

static void handle_write(PipeData &pipe_data, Message_Descriptor msg, std::vector<std::byte> message_data, pmos::Right reply_right)
{
    IPC_Write *write_msg = reinterpret_cast<IPC_Write *>(message_data.data());

    if (msg.size < sizeof(IPC_Write)) {
        kernelLogger() << "processd: Recieved very small write message\n" << frg::endlog;
        return;
    }

    if (!pipe_data.have_reader) {
        write_reply(reply_right, -EPIPE, 0);
        return;
    }

    size_t data_size = msg.size - sizeof(IPC_Write);
    bool noblock = write_msg->flags & IPC_FLAG_IO_OP_NONBLOCK;
    bool atomic = data_size <= PIPE_BUF;

    size_t bytes_written = 0;

    PendingWrite pending_write = {
        .reply_right = std::move(reply_right),
        .pending_data = frg::span<const uint8_t>(reinterpret_cast<const uint8_t *>(write_msg->data), data_size),
        .message_data = std::move(message_data),
        .noblock = noblock,
        .bytes_written = 0,
    };

    if (!pipe_data.pending_writes.empty()) {
        if (noblock) {
            write_reply(pending_write.reply_right, -EAGAIN, 0);
            return;
        } else {
            pipe_data.pending_writes.push_back(std::move(pending_write));
            return;
        }
    }

    while (pending_write.pending_data.size() > 0) {
        auto remaining_bytes = pending_write.pending_data.size();
        size_t needed = atomic ? remaining_bytes : 1;
        auto avail_space = pipe_data.buffer.available_space();

        if (avail_space < needed) {
            if (noblock) {
                break;
            } else {
                pending_write.bytes_written = bytes_written;
                pipe_data.pending_writes.push_back(std::move(pending_write));
                return;
            }
        } else {
            auto written = pipe_data.buffer.enqueue(pending_write.pending_data);
            bytes_written += written;
            pending_write.pending_data = pending_write.pending_data.subspan(written);
        }

        wakeup_readers(pipe_data);
    }

    if (atomic && bytes_written == 0) {
        write_reply(pending_write.reply_right, -EAGAIN, 0);
        return;
    } else {
        write_reply(pending_write.reply_right, 0, bytes_written);
    }
}

pmos::async::detached_task get_messages_pipe_writer(pmos::PortDispatcher &dispatcher, PipeData &pipe_data, pmos::RecieveRight rr)
{
    while (pipe_data.have_writer) {
        auto [msg, message, reply_right, _] = (co_await dispatcher.get_message(rr)).value();
    
        if (msg.size < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "processd: Recieved very small message\n" << frg::endlog;
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Recieve_Right_Destroyed_NUM:
            pipe_data.have_writer = false;
            break;

        case IPC_Write_NUM:
            handle_write(pipe_data, msg, message, std::move(reply_right));
            break;

        default:
            kernelLogger() << "processd: Unknown message type " << ipc_msg->type << " from pipe writer\n" << frg::endlog;
            break;
        }
    }

    wakeup_readers(pipe_data);
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

    PipeData pipe_data;
    get_messages_pipe_reader(dispatcher, pipe_data, std::move(read_right));
    get_messages_pipe_writer(dispatcher, pipe_data, std::move(write_right));

    dispatcher.dispatch();
}

void pipe_open(IPC_Pipe_Open &msg, pmos::Right reply_right)
{
    // kernelLogger() << "processd: Creating pipe\n" << frg::endlog;

    auto thread = std::thread(pipe_thread, msg, std::move(reply_right));
    thread.detach();
}