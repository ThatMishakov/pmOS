#include "pipe_handler.hh"
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdio.h>
#include <errno.h>
#include <system_error>
#include <thread>
#include "ipc.hh"
#include <cassert>

extern pmos_port_t main_port;

thread_local Pipe pipe_data;
thread_local auto service_port = create_port();

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

Pipe::ConsumerData &Pipe::register_new_consumer(uint64_t consumer_id, size_t refcount)
{
    auto &c = pipe_consumers[consumer_id];

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

    c.refcount += refcount;

    return c;
}

void pipe_main(IPC_Pipe_Open o, Message_Descriptor /* desc */)
{
    try {
        // This can probably throw when initializing pipe_data
        pipe_data.task_id = get_task_id();
        pipe_data.pipe_port = create_pipe_port();

        pipe_data.register_new_consumer(o.fs_consumer_id, 2);

        IPC_Pipe_Open_Reply r{
            .type = IPC_Pipe_Open_Reply_NUM,
            .flags = 0,
            .result_code = 0,
            .filesystem_id = o.fs_consumer_id,
            .reader_id = o.fs_consumer_id,
            .writer_id = o.fs_consumer_id,
            .pipe_port = pipe_data.pipe_port,
        };
        send_message(o.reply_port, r);
    } catch (std::system_error &e) {
        pipe_reply_error(o.reply_port, e.code().value());
        return;
    }

    bool cont = true;
    while (cont) {
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
    for (auto consumer: pipe_consumers) {
        if (consumer.second.refcount != 0)
            notify_unregister(consumer.first);
    }
}