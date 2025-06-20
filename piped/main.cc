#include "fs_consumer.hh"
#include "ipc.hh"
#include "pipe_handler.hh"

#include <cassert>
#include <cinttypes>
#include <memory>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdio.h>
#include <string>
#include <system_error>

pmos_port_t main_port = create_port();

std::string piped_port_name = "/pmos/piped";
pmos_right_t recieve_right  = 0;

int main()
{
    printf("Hello from piped! My PID: %li\n", get_task_id());

    {
        auto rr = create_right(main_port, &recieve_right, 0);
        if (rr.result != SUCCESS) {
            fprintf(stderr, "terminald: Error %i creating right\n", (int)rr.result);
            return 1;
        }

        result_t r = name_right(rr.right, piped_port_name.c_str(), piped_port_name.length(), 0);
        if (r != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(r) + " naming port\n";
            fprintf(stderr, "%s", error.c_str());
        }
    }

    while (true) {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        std::unique_ptr<char[]> msg_buff = std::make_unique<char[]>(msg.size);

        get_first_message(msg_buff.get(), 0, main_port);

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            fprintf(stderr, "Warning: recieved very small message\n");
            break;
        }

        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(msg_buff.get());

        switch (ipc_msg->type) {
        case IPC_Pipe_Open_NUM:
            if (msg.size < sizeof(IPC_Pipe_Open)) {
                fprintf(stderr,
                        "Warning: Received IPC_Pipe_Open from sender %" PRIu64
                        " is too small (expected %zu bytes, got %zu bytes)\n",
                        msg.sender, sizeof(IPC_Pipe_Open), msg.size);
                break;
            }
            open_pipe(std::move(msg_buff), msg);
            break;
        case IPCRegisterConsumerType: {
            assert(msg.size == sizeof(IPCPipeRegisterConsumer));
            IPCPipeRegisterConsumer *ipc_pipe_register_consumer =
                reinterpret_cast<IPCPipeRegisterConsumer *>(msg_buff.get());
            int result = 0;
            try {
                register_pipe_with_consumer(ipc_pipe_register_consumer->pipe_port,
                                            ipc_pipe_register_consumer->consumer_id);
            } catch (std::system_error &e) {
                result = e.code().value();
            }

            IPCPipeRegisterConsReply r {
                .type   = IPCPipeRegisterConsReplyType,
                .result = result,
            };
            send_message(ipc_pipe_register_consumer->reply_port, r);
        } break;
        case IPCNotifyUnregisterConsumerType: {
            assert(msg.size == sizeof(IPCNotifyUnregisterConsumer));
            IPCNotifyUnregisterConsumer *ipc_notify_unregister_consumer =
                reinterpret_cast<IPCNotifyUnregisterConsumer *>(msg_buff.get());
            try {
                unregister_pipe_with_consumer(ipc_notify_unregister_consumer->pipe_port,
                                              ipc_notify_unregister_consumer->consumer_id);
            } catch (...) {
            }
            break;
        }
        case IPC_Kernel_Group_Destroyed_NUM: {
            auto t = reinterpret_cast<IPC_Kernel_Group_Destroyed *>(msg_buff.get());
            if (msg.size < sizeof(IPC_Kernel_Group_Destroyed)) {
                fprintf(stderr,
                        "Warning: Received IPC_Kernel_Group_Destroyed from sender %" PRIu64
                        " is too small. Expected %zu bytes, got %zu bytes\n",
                        msg.sender, sizeof(IPC_Kernel_Group_Destroyed), msg.size);
                break;
            }

            if (t->task_group_id == 0) {
                fprintf(stderr,
                        "Warning: Recieved IPC_Kernel_Group_Task_Changed with group_id 0\n");
                break;
            }

            if (msg.sender != 0) {
                fprintf(stderr, "Warning: Recieved IPC_Kernel_Group_Task_Changed from userspace\n");
                break;
            }

            handle_consumer_destroyed(t->task_group_id);
            break;
        }
        default:
            fprintf(stderr, "Warning: Unknown message type %u (size %zu) from task %" PRIu64 "\n",
                    ipc_msg->type, msg.size, msg.sender);
            break;
            break;
        }
    }
}