#include "pipe_handler.hh"
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdio.h>
#include <errno.h>
#include <system_error>
#include <thread>
#include "ipc.hh"
#include <cassert>
#include <cstring>

extern pmos_port_t main_port;

thread_local Pipe pipe_data;
thread_local auto service_port = create_port();
thread_local bool pipe_continue = true;

void pipe_reply_error(uint64_t port, int errno_result)
{
    IPC_Pipe_Open_Reply r{
        .type = IPC_Pipe_Open_Reply_NUM,
        .flags = 0,
        .result_code = static_cast<int16_t>(-errno_result),
        .filesystem_id = 0,
        .reader_id = 0,
        .writer_id = 0,
        .pipe_port = 0,
    };

    send_message_port(port, sizeof r, reinterpret_cast<void *>(&r));
}

pmos_port_t create_pipe_port()
{
    auto r = create_port(TASK_ID_SELF, 0);
    if (r.result != SUCCESS)
        throw std::system_error(-r.result, std::system_category());
    return r.port;
}

Pipe::ConsumerData &Pipe::register_new_consumer(uint64_t consumer_id, size_t reader_refcount, size_t writer_refcount)
{
    auto &c = pipe_consumers[consumer_id] = std::make_unique<ConsumerData>();

    IPCPipeRegisterConsumer r{
        .reply_port = service_port,
        .pipe_port = pipe_port,
        .consumer_id = consumer_id,
    };
    send_message(main_port, r);

    Message_Descriptor desc;
    auto sr = syscall_get_message_info(&desc, service_port, 0);
    assert(sr == SUCCESS);
    assert(desc.size == sizeof(IPCPipeRegisterConsReply));

    IPCPipeRegisterConsReply reply;
    sr = get_first_message(reinterpret_cast<char *>(&reply), 0, service_port);
    assert(sr == SUCCESS);
    assert(reply.type == IPCPipeRegisterConsReplyType);

    if (reply.result < 0)
        throw std::system_error(-reply.result, std::system_category());

    c->reader_refcount += reader_refcount;
    c->writer_refcount += writer_refcount;

    return *c;
}

void read_reply_error(uint64_t port, int errno_result)
{
    IPC_Read_Reply r{
        .type = IPC_Read_Reply_NUM,
        .flags = 0,
        .result_code = static_cast<int16_t>(-errno_result),
    };
    send_message_port(port, sizeof r, reinterpret_cast<void *>(&r));
}

void read_pipe(std::unique_ptr<char []> ipc_msg, Message_Descriptor desc)
{
    try {
        const auto msg_size = desc.size;
        const auto sender_task_id = desc.sender;

        if (msg_size < sizeof(IPC_Read)) {
            fprintf(stderr, "Error: IPC_Read too small %li from task %li\n", msg_size, sender_task_id);
            return;
        }

        const auto &r = *reinterpret_cast<IPC_Read *>(ipc_msg.get());
        if (r.file_id != 0) {
            read_reply_error(r.reply_port, ENOENT);
            return;
        }

        if (r.max_size == 0) {
            read_reply_error(r.reply_port, EINVAL);
            return;
        }

        if (pipe_data.reader_refcount == 0) {
            read_reply_error(r.reply_port, EPIPE);
            return;
        }

        auto it = pipe_data.pipe_consumers.find(r.fs_consumer_id);
        if (it == pipe_data.pipe_consumers.end()) {
            read_reply_error(r.reply_port, EPIPE);
            return;
        }

        auto &consumer = *it->second;
        if (consumer.reader_refcount == 0) {
            read_reply_error(r.reply_port, EPIPE);
            return;
        }

        if (pipe_data.buffered_bytes == 0) {
            // If the pipe is closed, return 0 bytes
            if (pipe_data.writer_refcount == 0) {
                read_reply_error(r.reply_port, 0);
                return;
            }

            try {
                pipe_data.pending_readers.push_back(Pipe::PendingReader{
                    .consumer_id = r.fs_consumer_id,
                    .max_bytes = r.max_size,
                    .reply_port = r.reply_port,
                });

                consumer.pending_count++;
            } catch (std::bad_alloc &e) {
                fprintf(stderr, "Error adding reader to pending readers: %s\n", e.what());
                read_reply_error(r.reply_port, ENOMEM);
            }
            return;
        }

        const auto bytes_to_read = std::min(r.max_size, pipe_data.buffered_bytes);        
        std::unique_ptr<char []> reply_buff;
        try {
            reply_buff = std::make_unique<char[]>(sizeof(IPC_Read_Reply) + bytes_to_read);
        } catch (std::bad_alloc &e) {
            fprintf(stderr, "Error allocating read reply buffer: %s\n", e.what());
            read_reply_error(r.reply_port, ENOMEM);
            return;
        }
        auto *reply = reinterpret_cast<IPC_Read_Reply *>(reply_buff.get());
        reply->type = IPC_Read_Reply_NUM;
        reply->flags = 0;
        reply->result_code = 0;

        unsigned char * data = reply->data;
        pipe_data.fill_data(data, bytes_to_read);

        send_message(r.reply_port, reply, sizeof(IPC_Read_Reply) + bytes_to_read);

        pipe_data.pop_read_data(bytes_to_read);
    } catch (std::system_error &e) {
        fprintf(stderr, "Error sending read reply: %s\n", e.what());
        return;
    }
}

void unlock_readers()
{
    while (pipe_data.buffered_bytes > 0 and not pipe_data.pending_readers.empty()) {
        auto &consumer = pipe_data.pending_readers.front();
        const auto bytes_to_read = std::min(consumer.max_bytes, pipe_data.buffered_bytes);
        try {
            const auto size = sizeof(IPC_Read_Reply) + bytes_to_read;
            std::unique_ptr<char []> reply_buff = std::make_unique<char[]>(size);
            auto *reply = reinterpret_cast<IPC_Read_Reply *>(reply_buff.get());
            reply->type = IPC_Read_Reply_NUM;
            reply->flags = 0;
            reply->result_code = 0;
            memcpy(reply->data, pipe_data.buffered_messages.front().message.get() + pipe_data.buffered_messages.front().start_position, bytes_to_read);
            send_message(consumer.reply_port, reply, size);
        } catch (std::system_error &e) {
            fprintf(stderr, "Error sending read reply: %s\n", e.what());
            pipe_data.pending_readers.pop_front();
            continue;
        }
        pipe_data.pop_read_data(bytes_to_read);
        pipe_data.pending_readers.pop_front();
    }
}

void write_reply_error(uint64_t port, int errno_result)
{
    IPC_Write_Reply r{
        .type = IPC_Write_Reply_NUM,
        .flags = 0,
        .result_code = static_cast<int16_t>(-errno_result),
        .bytes_written = 0,
    };
    send_message_port(port, sizeof r, reinterpret_cast<void *>(&r));
}

void write_pipe(std::unique_ptr<char []> ipc_msg, Message_Descriptor desc)
{
    try {
        const auto msg_size = desc.size;
        const auto sender_task_id = desc.sender;
        if (msg_size < sizeof(IPC_Write)) {
            fprintf(stderr, "Error: IPC_Write too small %li from task %li\n", msg_size, sender_task_id);
            return;
        }

        auto r = *reinterpret_cast<IPC_Write *>(ipc_msg.get());
        if (r.file_id != 1) {
            write_reply_error(r.reply_port, ENOENT);
            return;
        }

        const auto data_size = desc.size - sizeof(IPC_Write);
        if (data_size == 0) {
            write_reply_error(r.reply_port, EINVAL);
            return;
        }

        if (pipe_data.writer_refcount == 0) {
            write_reply_error(r.reply_port, EPIPE);
            return;
        }

        auto it = pipe_data.pipe_consumers.find(r.fs_consumer_id);
        if (it == pipe_data.pipe_consumers.end()) {
            write_reply_error(r.reply_port, EPIPE);
            return;
        }

        auto &consumer = *it->second;
        if (consumer.writer_refcount == 0) {
            write_reply_error(r.reply_port, EPIPE);
            return;
        }

        if (pipe_data.reader_refcount == 0) {
            write_reply_error(r.reply_port, 0);
            return;
        }

        try {
            pipe_data.buffered_messages.push_back(MessageBuffer{
                .message = std::move(ipc_msg),
                .start_position = sizeof(IPC_Write),
                .size = desc.size,
            });
        } catch (std::bad_alloc &e) {
            write_reply_error(r.reply_port, ENOMEM);
            return;
        }

        pipe_data.buffered_bytes += data_size;
        unlock_readers();

        IPC_Write_Reply reply{
            .type = IPC_Write_Reply_NUM,
            .flags = 0,
            .result_code = 0,
            .bytes_written = data_size,
        };
        send_message(r.reply_port, reply);
    } catch (std::system_error &e) {
        fprintf(stderr, "Error sending write reply: %s\n", e.what());
        return;
    }
}

void fail_consumer_readers(uint64_t consumer_id)
{
    for (auto it = pipe_data.pending_readers.begin(); it != pipe_data.pending_readers.end(); ) {
        auto c = it++;
        if (c->consumer_id == consumer_id) {
            try {
                read_reply_error(c->reply_port, 0);
            } catch (...) {}
            pipe_data.pending_readers.erase(c);
        }
    }
}

void pop_all_readers()
{
    for (auto it = pipe_data.pending_readers.begin(); it != pipe_data.pending_readers.end(); ) {
        auto c = it++;
        try {
            read_reply_error(c->reply_port, 0);
        } catch (...) {}
        pipe_data.pending_readers.erase(c);
    }
}

void clear_buffered_messages()
{
    pipe_data.buffered_messages.clear();
    pipe_data.buffered_bytes = 0;
}

void close_pipe(std::unique_ptr<char []> ipc_msg, Message_Descriptor desc)
{
    const auto msg_size = desc.size;
    const auto sender_task_id = desc.sender;
    if (msg_size < sizeof(IPC_Close)) {
        fprintf(stderr, "Error: IPC_Close too small %li from task %li\n", msg_size, sender_task_id);
        return;
    }

    const auto &c = *reinterpret_cast<IPC_Close *>(ipc_msg.get());
    if (c.file_id > 1) {
        fprintf(stderr, "Error: IPC_Close bogus file_id %li from task %li\n", c.file_id, sender_task_id);
        return;
    }

    auto consumer_it = pipe_data.pipe_consumers.find(c.fs_consumer_id);
    if (consumer_it == pipe_data.pipe_consumers.end())
        return; // Consumer already closed

    auto &consumer = *consumer_it->second;
    switch (c.file_id) {
    case 0:
        if (consumer.reader_refcount == 0)
            return;
        consumer.reader_refcount--;
        pipe_data.reader_refcount--;
        break;
    case 1:
        if (consumer.writer_refcount == 0)
            return;
        consumer.writer_refcount--;
        pipe_data.writer_refcount--;
        break;
    default:
        assert(false);
    }

    if (consumer.reader_refcount == 0 and c.file_id == 0) {
        for (auto it = pipe_data.pending_readers.begin(); it != pipe_data.pending_readers.end(); ) {
            auto c = it++;
            if (c->consumer_id == consumer_it->first) {
                try {
                    read_reply_error(c->reply_port, 0);
                } catch (...) {}
                pipe_data.pending_readers.erase(c);
            }
        }
    }

    if (pipe_data.writer_refcount == 0) {
        pop_all_readers();
    }

    if (pipe_data.reader_refcount == 0) {
        clear_buffered_messages();
    }

    if (consumer.reader_refcount == 0 and consumer.writer_refcount == 0) {
        pipe_data.pipe_consumers.erase(consumer_it);
        pipe_data.notify_unregister(c.fs_consumer_id);
    }

    if (pipe_data.pipe_consumers.empty())
        // Exit
        pipe_continue = false;
}

void cleanup_consumer(std::unique_ptr<char []> ipc_msg, Message_Descriptor desc)
{
    const auto msg_size = desc.size;
    const auto sender_task_id = desc.sender;
    assert(msg_size == sizeof(IPCNotifyConsumerDestroyed));

    // TODO: Check that the message comes from the correct task
    (void)sender_task_id;

    const auto &c = *reinterpret_cast<IPCNotifyConsumerDestroyed *>(ipc_msg.get());
    auto it = pipe_data.pipe_consumers.find(c.consumer_id);
    if (it == pipe_data.pipe_consumers.end())
        // Consumer already closed
        return;

    auto &consumer = *it->second;
    pipe_data.reader_refcount -= consumer.reader_refcount;
    pipe_data.writer_refcount -= consumer.writer_refcount;

    // Pop all consumer readers
    if (consumer.reader_refcount != 0)
        fail_consumer_readers(c.consumer_id);

    // If there are no more writers, pop all pending readers
    if (pipe_data.writer_refcount == 0)
        pop_all_readers();

    fail_consumer_readers(c.consumer_id);

    // Clear the buffered messages
    if (pipe_data.reader_refcount == 0) {
        clear_buffered_messages();
    }

    pipe_data.pipe_consumers.erase(it);

    if (pipe_data.pipe_consumers.empty())
        // Exit
        pipe_continue = false;
}    


void Pipe::fill_data(unsigned char * data, size_t bytes_to_read) noexcept
{
    size_t bytes_read = 0;
    for (size_t i = 0; i < buffered_messages.size(); i++) {
        auto &msg = buffered_messages[i];
        const auto bytes_to_copy = std::min(bytes_to_read - bytes_read, msg.size - msg.start_position);
        memcpy(data + bytes_read, msg.message.get() + msg.start_position, bytes_to_copy);
        bytes_read += bytes_to_copy;
        if (bytes_read == bytes_to_read)
            break;
    }
}

void Pipe::pop_read_data(size_t bytes_to_pop) noexcept
{
    size_t bytes_popped = 0;
    while (bytes_popped < bytes_to_pop) {
        auto &msg = buffered_messages.front();
        const auto bytes_to_pop_from_msg = std::min(bytes_to_pop - bytes_popped, msg.size - msg.start_position);
        msg.start_position += bytes_to_pop_from_msg;
        bytes_popped += bytes_to_pop_from_msg;
        if (msg.start_position == msg.size)
            buffered_messages.pop_front();
    }
    buffered_bytes -= bytes_to_pop;
}

void pipe_main(IPC_Pipe_Open o, Message_Descriptor /* desc */)
{
    try {
        // This can probably throw when initializing pipe_data
        pipe_data.task_id = get_task_id();
        pipe_data.pipe_port = create_pipe_port();

        pipe_data.register_new_consumer(o.fs_consumer_id, 1, 1);

        pipe_data.reader_refcount = 1;
        pipe_data.writer_refcount = 1;

        IPC_Pipe_Open_Reply r{
            .type = IPC_Pipe_Open_Reply_NUM,
            .flags = 0,
            .result_code = 0,
            .filesystem_id = o.fs_consumer_id,
            .reader_id = 0,
            .writer_id = 1,
            .pipe_port = pipe_data.pipe_port,
        };
        send_message(o.reply_port, r);
    } catch (std::system_error &e) {
        pipe_reply_error(o.reply_port, e.code().value());
        return;
    }

    while (pipe_continue) {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, pipe_data.pipe_port, 0);

        std::unique_ptr<char[]> msg_buff = std::make_unique<char[]>(msg.size);

        auto r = get_first_message(msg_buff.get(), 0, pipe_data.pipe_port);
        if (r != SUCCESS) {
            printf("Could not get the message\n");
            break;
        }

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            fprintf(stderr, "Warning: recieved very small message\n");
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(msg_buff.get());

        switch (ipc_msg->type) {
        case IPC_Read_NUM: {
            read_pipe(std::move(msg_buff), msg);
            break;
        }
        case IPC_Write_NUM: {
            write_pipe(std::move(msg_buff), msg);
            break;
        }
        case IPC_Close_NUM: {
            close_pipe(std::move(msg_buff), msg);
            break;
        }
        case IPCNotifyConsumerDestroyedType: {
            cleanup_consumer(std::move(msg_buff), msg);
            break;
        }
        default:
            fprintf(stderr, ("Pipe " + std::to_string(pipe_data.pipe_port) + ": Unknown message type " + std::to_string(ipc_msg->type) + " from task " + std::to_string(msg.sender) + "\n").c_str());
            break;
        }
    }
}

void open_pipe(std::unique_ptr<char []> ipc_msg, Message_Descriptor desc)
{
    const auto msg_size = desc.size;
    const auto sender_task_id = desc.sender;
    if (msg_size < sizeof(IPC_Pipe_Open)) {
        fprintf(stderr, "Error: IPC_Pipe_Open too small %li from task %li\n", msg_size, sender_task_id);
        return;
    }

    const auto &p = *reinterpret_cast<IPC_Pipe_Open *>(ipc_msg.get());

    // Verify the consumer
    syscall_r res = is_task_group_member(sender_task_id, p.fs_consumer_id);
    if (res.result != SUCCESS or res.value != 1) {
        pipe_reply_error(p.reply_port, EPERM);
        return;
    }

    try {
        std::thread t(pipe_main, p, desc);
        t.detach();
    } catch (std::system_error &e) {
        pipe_reply_error(p.reply_port, e.code().value());
        return;
    }
}

void Pipe::notify_unregister(uint64_t consumer_id)
{
    IPCNotifyUnregisterConsumer n{
        .pipe_port = pipe_port,
        .consumer_id = consumer_id,
    };
    send_message(main_port, n);
}

Pipe::~Pipe()
{
    for (const auto &consumer: pipe_consumers) {
        if (consumer.second->reader_refcount != 0 or consumer.second->writer_refcount != 0)
            notify_unregister(consumer.first);
    }
}