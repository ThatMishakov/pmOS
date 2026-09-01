#include "pipe.hh"
#include "log.hh"
#include <thread>
#include <memory>
#include <limits.h>
#include <deque>

#include <pmos/async/coroutines.hh>
#include <pmos/containers/ring_buffer.hh>

struct PendingWrite {
    pmos::Right reply_right;
    std::span<const uint8_t> pending_data;
    std::vector<std::byte> message_data;
    size_t bytes_written;
};

struct PendingRead {
    pmos::Right reply_right;
    size_t max_size;
};

constexpr size_t PIPE_SIZE = 65536; // 64 KiB

struct PipeData {
    bool have_reader = true;
    bool have_writer = true;
    pmos::containers::byte_ring_buffer<PIPE_SIZE> buffer;
    std::deque<PendingWrite> pending_writes;
    std::deque<PendingRead> pending_reads;
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
        kernelLogger() << "posix: Error " << send_result.error() << " sending message to port " << reply_right.get() << " for pipe_write\n" << frg::endlog;
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

static void wakeup_readers(PipeData &pipe_data)
{
    while (!pipe_data.pending_reads.empty() && !pipe_data.buffer.empty()) {
        auto pending_read = std::move(pipe_data.pending_reads.front());
        pipe_data.pending_reads.pop_front();
        const size_t size = std::min(pending_read.max_size, pipe_data.buffer.size());
        std::vector<uint8_t> data(sizeof(IPC_Read_Reply) + size);
        IPC_Read_Reply *reply = reinterpret_cast<IPC_Read_Reply *>(data.data());

        reply->type = IPC_Read_Reply_NUM;
        reply->flags = 0;
        reply->result_code = 0;

        std::span<uint8_t> data_span(data.data() + sizeof(IPC_Read_Reply), size);
        pipe_data.buffer.peek(data_span);

        auto send_result = pmos::send_message_right(pending_read.reply_right, std::span(data), {}, true);
        if (!send_result) {
            kernelLogger() << "posix: Error " << send_result.error() << " sending message to port " << pending_read.reply_right.get() << " for pipe_read\n" << frg::endlog;
        } else {
            pipe_data.buffer.pop_bytes(size);
        }
    }

    if (!pipe_data.have_writer) {
        while (!pipe_data.pending_reads.empty()) {
            auto pending_read = std::move(pipe_data.pending_reads.front());
            pipe_data.pending_reads.pop_front();
            IPC_Read_Reply reply = {
                .type        = IPC_Read_Reply_NUM,
                .flags       = 0,
                .result_code = 0,
            };
            auto send_result = pmos::send_message_right_one(pending_read.reply_right, reply, {}, true);
            if (!send_result) {
                kernelLogger() << "posix: Error " << send_result.error() << " sending message to port " << pending_read.reply_right.get() << " for pipe_read\n" << frg::endlog;
            }
        }
    }
}

static void wakeup_writers(PipeData &pipe_data)
{
    while (!pipe_data.pending_writes.empty()) {
        auto &pending_write = pipe_data.pending_writes.front();
        while (pending_write.pending_data.size() > 0) {
            auto remaining_bytes = pending_write.pending_data.size();
            size_t needed = remaining_bytes <= PIPE_BUF ? remaining_bytes : 1;
            auto avail_space = pipe_data.buffer.available_space();

            if (avail_space < needed) {
                break;
            } else {
                auto written = pipe_data.buffer.enqueue(pending_write.pending_data);
                pending_write.bytes_written += written;
                pending_write.pending_data = pending_write.pending_data.subspan(written);
            }
        }

        if (pending_write.pending_data.size() == 0) {
            write_reply(pending_write.reply_right, 0, pending_write.bytes_written);
            pipe_data.pending_writes.pop_front();
        } else {
            break;
        }
    }
}

static void handle_write(PipeData &pipe_data, Message_Descriptor msg, std::vector<std::byte> message_data, pmos::Right reply_right)
{
    IPC_Write *write_msg = reinterpret_cast<IPC_Write *>(message_data.data());

    if (msg.size < sizeof(IPC_Write)) {
        kernelLogger() << "posix: Received very small write message\n" << frg::endlog;
        return;
    }

    if (write_msg->flags & IPC_FLAG_IO_OP_SEEK) {
        write_reply(reply_right, -ESPIPE, 0);
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
        .pending_data = std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(write_msg->data), data_size),
        .message_data = std::move(message_data),
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

static void handle_read(PipeData &pipe_data, Message_Descriptor msg, std::vector<std::byte> message_data, pmos::Right reply_right)
{
    IPC_Read *read_msg = reinterpret_cast<IPC_Read *>(message_data.data());

    if (msg.size < sizeof(IPC_Read)) {
        kernelLogger() << "posix: Received very small read message\n" << frg::endlog;
        return;
    }

    if (read_msg->flags & IPC_FLAG_IO_OP_SEEK) {
        // Seeking on a pipe
        IPC_Read_Reply reply = {
            .type        = IPC_Read_Reply_NUM,
            .flags       = 0,
            .result_code = -ESPIPE,
        };
        auto send_result = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!send_result) {
            kernelLogger() << "posix: Error " << send_result.error() << " sending message to port " << reply_right.get() << " for pipe_read\n" << frg::endlog;
        }
        return;
    }

    if (!pipe_data.have_writer && pipe_data.buffer.empty()) {
        IPC_Read_Reply reply = {
            .type        = IPC_Read_Reply_NUM,
            .flags       = 0,
            .result_code = 0,
        };

        auto send_result = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!send_result) {
            kernelLogger() << "posix: Error " << send_result.error() << " sending message to port " << reply_right.get() << " for pipe_read\n" << frg::endlog;
        }
        return;
    }

    const size_t max_size = read_msg->max_size;

    if (pipe_data.buffer.empty()) {
        if (read_msg->flags & IPC_FLAG_IO_OP_NONBLOCK) {
            IPC_Read_Reply reply = {
                .type        = IPC_Read_Reply_NUM,
                .flags       = 0,
                .result_code = -EAGAIN,
            };

            auto send_result = pmos::send_message_right_one(reply_right, reply, {}, true);
            if (!send_result) {
                kernelLogger() << "posix: Error " << send_result.error() << " sending message to port " << reply_right.get() << " for pipe_read\n" << frg::endlog;
            }
            return;
        } else {
            PendingRead pending_read = {
                .reply_right = std::move(reply_right),
                .max_size = max_size,
            };
            pipe_data.pending_reads.push_back(std::move(pending_read));
            return;
        }
    }

    const size_t size = std::min(max_size, pipe_data.buffer.size());
    std::vector<uint8_t> data(sizeof(IPC_Read_Reply) + size);
    IPC_Read_Reply *reply = reinterpret_cast<IPC_Read_Reply *>(data.data());

    reply->type = IPC_Read_Reply_NUM;
    reply->flags = 0;
    reply->result_code = 0;

    std::span<uint8_t> data_span(data.data() + sizeof(IPC_Read_Reply), size);
    pipe_data.buffer.peek(data_span);

    auto send_result = pmos::send_message_right(reply_right, std::span(data), {}, true);
    if (!send_result) {
        kernelLogger() << "posix: Error " << send_result.error() << " sending message to port " << reply_right.get() << " for pipe_read\n" << frg::endlog;
    } else {
        pipe_data.buffer.pop_bytes(size);
        wakeup_writers(pipe_data);
    }
}

pmos::async::detached_task get_messages_pipe_writer(pmos::PortDispatcher &dispatcher, PipeData &pipe_data, pmos::ReceiveRight rr)
{
    while (pipe_data.have_writer) {
        auto [msg, message, reply_right, _] = (co_await dispatcher.get_message(rr)).value();
    
        if (msg.size < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posix: Received very small message\n" << frg::endlog;
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            pipe_data.have_writer = false;
            break;

        case IPC_Write_NUM:
            handle_write(pipe_data, msg, message, std::move(reply_right));
            break;

        default:
            kernelLogger() << "posix: Unknown message type " << ipc_msg->type << " from pipe writer\n" << frg::endlog;
            break;
        }
    }

    wakeup_readers(pipe_data);
}

pmos::async::detached_task get_messages_pipe_reader(pmos::PortDispatcher &dispatcher, PipeData &pipe_data, pmos::ReceiveRight rr)
{
    while (pipe_data.have_reader) {
        auto [msg, message, reply_right, _] = (co_await dispatcher.get_message(rr)).value();
    
        if (msg.size < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posix: Received very small message\n" << frg::endlog;
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            pipe_data.have_reader = false;
            break;

        case IPC_Read_NUM:
            handle_read(pipe_data, msg, std::move(message), std::move(reply_right));
            break;

        default:
            kernelLogger() << "posix: Unknown message type " << ipc_msg->type << " from pipe reader\n" << frg::endlog;
            break;
        }
    }

    epipe_writers(pipe_data);
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
        kernelLogger() << "posix: Error " << send_result.error() << " sending message to port " << reply_right.get() << " for pipe_open\n" << frg::endlog;
        return;
    }

    PipeData pipe_data;
    get_messages_pipe_reader(dispatcher, pipe_data, std::move(read_right));
    get_messages_pipe_writer(dispatcher, pipe_data, std::move(write_right));

    dispatcher.dispatch();
}

void pipe_open(IPC_Pipe_Open &msg, pmos::Right reply_right)
{
    // kernelLogger() << "posix: Creating pipe\n" << frg::endlog;

    auto thread = std::thread(pipe_thread, msg, std::move(reply_right));
    thread.detach();
}