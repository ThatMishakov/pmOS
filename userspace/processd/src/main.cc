/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cassert>
#include <cinttypes>
#include <memory>
#include <pmos/helpers.hh>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <pmos/async/coroutines.hh>
#include "log.hh"
#include <string.h>
#include <charconv>
#include "pipe.hh"
#include "vfs.hh"
#include "process.hh"

void KernelSink::operator()(const char *message)
{
    pmos_kernel_debug_log(message, strlen(message));
}

pmos::Port main_port = pmos::Port::create().value();
pmos::PortDispatcher dispatcher(main_port);

pmos::async::detached_task vfs_handle_messages();

void sigaction_reply(pmos::Right reply_right, int result, uint32_t sa_flags = 0, uint32_t sa_handler = 0, uint64_t sa_restorer = 0, uint64_t sa_mask = 0)
{
    IPC_Sigaction_Reply reply = {
        .type = IPC_Sigaction_Reply_NUM,
        .flags = 0,
        .result = result,
        .old_sa_flags = sa_flags,
        .old_sa_handler = sa_handler,
        .old_sa_restorer = sa_restorer,
        .old_sa_mask = sa_mask
    };

    auto r = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!r)
        kernelLogger() << "processd: Error " << r.error() << " sending message for sigaction_reply\n" << frg::endlog;
}

void sigaction_handle(std::shared_ptr<Process> process, pmos::Right reply_right, IPC_Sigaction *msg)
{
    auto num = msg->sigval;
    if (num <= 0 || num >= 65) {
        sigaction_reply(std::move(reply_right), EINVAL);
        return;
    }
    num -= 1; // Convert to 0-based index

    auto sigaction = process->sigactions[num];
    if (msg->flags & SIGACTION_FLAG_SET) {
        process->sigactions[num] = {
            .sa_handler = msg->sa_handler_,
            .sa_restorer = msg->sa_restorer,
            .sa_mask = msg->sa_mask,
            .sa_flags = msg->sa_flags,
        };
    }

    sigaction_reply(std::move(reply_right), 0, sigaction.sa_flags, sigaction.sa_handler, sigaction.sa_restorer, sigaction.sa_mask);
}

pmos::async::detached_task handle_process_messages(pmos::ReceiveRight rr, std::shared_ptr<Process> process)
{
    while (1) {
        auto [msg, message, reply_right, _] = (co_await dispatcher.get_message(rr)).value();
    
        if (msg.size < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "processd: Received very small message\n" << frg::endlog;
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            co_return;

        case IPC_Open_NUM: {
            if (message.size() < sizeof(IPC_Open)) {
                kernelLogger() << "posixd: Received IPC_Open that is too small from task " << msg.sender << " of size " << message.size() << "\n" << frg::endlog;
                break;
            }

            auto *m = reinterpret_cast<IPC_Open *>(message.data());
            std::string path(m->path, message.size() - sizeof(IPC_Open));
            open_file(std::move(reply_right), path);
        } break;

        case IPC_Stat_NUM: {
            if (message.size() < sizeof(IPC_Stat)) {
                kernelLogger() << "posixd: Received IPC_Stat that is too small while attending file\n" << frg::endlog;
                break;
            }
            auto *stat_msg = reinterpret_cast<IPC_Stat *>(message.data());

            std::string stat_msg_path(stat_msg->path, message.size() - sizeof(IPC_Stat));

            stat_handle(nullptr, std::move(reply_right), stat_msg->flags, std::move(stat_msg_path));
        }
            break;

        case IPC_Sigaction_NUM: {
            if (message.size() < sizeof(IPC_Sigaction)) {
                kernelLogger() << "posixd: Received IPC_Sigaction that is too small while attending file\n" << frg::endlog;
                break;
            }
            auto *sigaction_msg = reinterpret_cast<IPC_Sigaction *>(message.data());

            sigaction_handle(process, std::move(reply_right), sigaction_msg);
        }
            break;

        default:
            kernelLogger() << "processd: Unknown message type " << ipc_msg->type << " from process\n" << frg::endlog;
            break;
        }
    }
}

void register_process(IPC_Register_Process *msg, pmos::Right reply_right)
{
    (void)msg;

    // TODO
    auto process = std::make_shared<Process>();

    auto right = main_port.create_right(pmos::RightType::SendMany);
    auto [send_right, receive_right] = std::move(right.value());
    handle_process_messages(std::move(receive_right), process);

    IPC_Register_Process_Reply reply = {
        .type = IPC_Register_Process_Reply_NUM,
        .flags = 0,
        .result = 0,
        .pid = process->pid, // PID is TODO (as everything else here)
    };

    auto r = pmos::send_message_right_one(reply_right, reply, {}, true, std::move(send_right));
    if (!r)
        kernelLogger() << "processd: Error " << r.error() << " sending message to right " << reply_right.get() << " for register_process\n" << frg::endlog;
}

pmos::async::detached_task get_messages_bootstrapd(pmos::ReceiveRight rr)
{
    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message(rr)).value();
    
        if (msg.size < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "processd: Received very small message\n" << frg::endlog;
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            co_return;

        case IPC_Pipe_Open_NUM: {
            if (msg.size < sizeof(IPC_Pipe_Open)) {
                kernelLogger() << "processd: Received IPC_Pipe_Open that is too small from task " << msg.sender << " of size " << msg.size << "\n" << frg::endlog;
                break;
            }

            IPC_Pipe_Open *m = reinterpret_cast<IPC_Pipe_Open *>(ipc_msg);
            pipe_open(*m, std::move(reply_right));
            break;
        }
        case IPC_Mount_FS_NUM: {
            if (message.size() < sizeof(IPC_Mount_FS)) {
                kernelLogger() << "posixd: Received IPC_Mount_FS that is too small from task " << msg.sender << " of size " << message.size() << "\n" << frg::endlog;
                break;
            }

            auto *m = reinterpret_cast<IPC_Mount_FS *>(message.data());
            std::string mountpoint(m->mount_path, message.size() - sizeof(IPC_Mount_FS));
            mount_filesystem(std::move(reply_right), std::move(rights[0]), mountpoint, m->root_fd);
        } break;
        case IPC_Register_Process_NUM: {
            if (msg.size < sizeof(IPC_Register_Process)) {
                kernelLogger() << "processd: Received IPC_Register_Process that is too small from task " << msg.sender << " of size " << msg.size << "\n" << frg::endlog;
                break;
            }

            IPC_Register_Process *m = reinterpret_cast<IPC_Register_Process *>(ipc_msg);
            register_process(m, std::move(reply_right));
            break;
        }
        default:
            kernelLogger() << "processd: Unknown message type " << ipc_msg->type << " from bootstrapd\n" << frg::endlog;
            break;
        }
    }
}

void parse_args(int argc, char *argv[])
{
    if (argc != 3) {
        kernelLogger() << "processd: Unexpected number of arguments " << argc << " (expected 3). Not replying to bootstrapd...\n" << frg::endlog;
        return;
    }

    if (std::string_view(argv[1]) != "--bootstrapd_right") {
        kernelLogger() << "processd: Unexpected argument " << argv[1] << " (expected --bootstrapd_right)\n" << frg::endlog;
        return;
    }

    const auto numb = argv[2];
    const auto numb_end = numb + strlen(numb);

    pmos_right_t right_number;
    auto [ptr, ec] = std::from_chars(numb, numb_end, right_number);
    if (ec != std::errc()) {
        kernelLogger() << "processd: Error parsing the reply port " << std::make_error_code(ec).message() << "\n" << frg::endlog;
        return;
    }
    if (ptr != numb_end) {
        kernelLogger() << "processd: Error parsing the reply port, symbols after number\n" << frg::endlog;
        return;
    }

    auto pr = pmos::Right::from(right_number);
    if (!pr) {
        kernelLogger() << "processd: Error getting right from the arguments " << pr.error() << "\n" << frg::endlog;
        return;
    }

    auto right = main_port.create_right(pmos::RightType::SendMany);
    auto [send_right, receive_right] = std::move(right.value());
    get_messages_bootstrapd(std::move(receive_right));

    IPC_Request_Right_Reply reply = {
        .type = IPC_Request_Right_Reply_NUM,
        .flags = 0,
        .result = 0,
    };

    send_message_right_one(pr.value(), reply, {}, true, std::move(send_right));
}

int main(int argc, char *argv[])
{
    kernelLogger() << "processd started\n" << frg::endlog;
    parse_args(argc, argv);

    // get_messages();
    // vfs_handle_messages();
    dispatcher.dispatch();
    return 0;
}