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

#include "fs_ops.hh"
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
#include <pmos/utility/scope_guard.hh>
#include "log.hh"
#include <string.h>
#include <charconv>
#include "pipe.hh"
#include "vfs.hh"
#include "process.hh"
#include "pty.hh"
#include "devfs.hh"
#include <elf.h>
#include <pmos/memory.h>

void KernelSink::operator()(const char *message)
{
    pmos_kernel_debug_log(message, strlen(message));
}

pmos::Port main_port = pmos::Port::create().value();
pmos::PortDispatcher dispatcher(main_port);

pmos::async::detached_task vfs_handle_messages();

void register_process(IPC_Register_Process *msg, pmos::Right reply_right, std::shared_ptr<Process> process, pmos::Right process_right);

void execve_reply(pmos::Right reply_right, int error)
{
    kernelLogger() << "processd error " << error << "\n" << frg::endlog;

    IPC_Execve_Reply reply = {
        .type = IPC_Execve_Reply_NUM,
        .result_code = error,
    };

    auto r = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!r)
        kernelLogger() << "processd: Error " << r.error().first << " sending message to right " << reply_right.get() << " for execve_reply\n" << frg::endlog;
}

bool sum_less_or_equals(uint64_t a, uint64_t b, uint64_t size)
{
    return (a + b >= a) && (a + b <= size);
}

#define ELF_ENDIANNESS 1
#ifdef __x86_64__
#define ELF_INSTR_SET EM_X86_64
#elif defined(__i386__)
#define ELF_INSTR_SET EM_386
#elif defined(__riscv)
#define ELF_INSTR_SET EM_RISCV
#elif defined(__loongarch__)
#define ELF_INSTR_SET EM_LOONGARCH
#endif

pmos::async::task<std::expected<std::optional<std::string>, int>> get_interpreter_path(pmos::Right &file_handle)
{
    Elf32_Ehdr ehdr;

    auto read_result = co_await read_file(file_handle, std::span<uint8_t>((uint8_t *)&ehdr, sizeof(ehdr)), 0);
    if (!read_result)
        co_return std::unexpected(read_result.error());

    if (ehdr.e_ident[4] == R_LARCH_32) {
        uint32_t phdr_size = ehdr.e_phentsize;
        uint32_t phdr_count = ehdr.e_phnum;
        uint32_t total_size = phdr_size * phdr_count;
        std::vector<uint8_t> phdr_data(total_size);
        auto phdr_read_result = co_await read_file(file_handle, phdr_data, ehdr.e_phoff);
        if (!phdr_read_result)
            co_return std::unexpected((int)phdr_read_result.error());

        for (uint32_t i = 0; i < phdr_count; ++i) {
            Elf32_Phdr *phdr = reinterpret_cast<Elf32_Phdr *>(phdr_data.data() + i * phdr_size);
            if (phdr->p_type == PT_INTERP) {
                std::vector<char> interp_path(phdr->p_filesz);
                auto interp_read_result = co_await read_file(file_handle, std::span<uint8_t>((uint8_t *)interp_path.data(), interp_path.size()), phdr->p_offset);
                if (!interp_read_result)
                    co_return std::unexpected(interp_read_result.error());
                co_return std::string(interp_path.data(), interp_path.size());
            }
        }
    } else if (ehdr.e_ident[4] == R_LARCH_64) {
        Elf64_Ehdr ehdr64;
        auto read_result = co_await read_file(file_handle, std::span<uint8_t>((uint8_t *)&ehdr64, sizeof(ehdr64)), 0);
        if (!read_result)
            co_return std::unexpected((int)read_result.error());

        uint64_t phdr_size = ehdr64.e_phentsize;
        uint64_t phdr_count = ehdr64.e_phnum;
        uint64_t total_size = phdr_size * phdr_count;
        std::vector<uint8_t> phdr_data(total_size);
        auto phdr_read_result = co_await read_file(file_handle, phdr_data, ehdr64.e_phoff);
        if (!phdr_read_result)
            co_return std::unexpected((int)phdr_read_result.error());

        for (uint64_t i = 0; i < phdr_count; ++i) {
            Elf64_Phdr *phdr = reinterpret_cast<Elf64_Phdr *>(phdr_data.data() + i * phdr_size);
            if (phdr->p_type == PT_INTERP) {
                std::vector<char> interp_path(phdr->p_filesz);
                auto interp_read_result = co_await read_file(file_handle, std::span<uint8_t>((uint8_t *)interp_path.data(), interp_path.size()), phdr->p_offset);
                if (!interp_read_result)
                    co_return std::unexpected(interp_read_result.error());
                co_return std::string(interp_path.data(), interp_path.size());
            }
        }
    } else {
        co_return std::unexpected(-ENOEXEC);
    }

    co_return {};
}

pmos::async::task<std::expected<void, int>> load_executable(uint64_t task_id, pmos::Right &file_handle, std::shared_ptr<Process> process)
{
    Elf32_Ehdr ehdr;

    auto read_result = co_await read_file(file_handle, std::span<uint8_t>((uint8_t *)&ehdr, sizeof(ehdr)), 0);
    if (!read_result) {
        co_return std::unexpected((int)read_result.error());
    }

    if (memcmp(&ehdr.e_ident, ELFMAG, SELFMAG))
        co_return std::unexpected(-ENOEXEC);

    if (ehdr.e_ident[5] != ELF_ENDIANNESS)
        co_return std::unexpected(-ENOEXEC);

    if (ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN)
        co_return std::unexpected(-ENOEXEC);

    auto interp_path_result = co_await get_interpreter_path(file_handle);
    if (!interp_path_result)
        co_return std::unexpected(interp_path_result.error());
    
    auto interp = std::move(interp_path_result.value());
    pmos::Right interp_handle;
    bool interp_is_relocatable = false;
    if (interp) {
        auto interp_file = co_await get_file_handle(*interp, process);
        if (!interp_file)
            co_return std::unexpected(interp_file.error());
        interp_handle = std::move(interp_file.value());

        Elf32_Ehdr interp_ehdr;
        auto interp_read_result = co_await read_file(interp_handle, std::span<uint8_t>((uint8_t *)&interp_ehdr, sizeof(interp_ehdr)), 0);
        if (!interp_read_result)
            co_return std::unexpected((int)interp_read_result.error());

        if (memcmp(&interp_ehdr.e_ident, ELFMAG, SELFMAG))
            co_return std::unexpected(-ENOEXEC);
        if (interp_ehdr.e_ident[5] != ELF_ENDIANNESS)
            co_return std::unexpected(-ENOEXEC);
        if (interp_ehdr.e_type != ET_EXEC && interp_ehdr.e_type != ET_DYN)
            co_return std::unexpected(-ENOEXEC);

        if (interp_ehdr.e_machine != ehdr.e_machine)
            co_return std::unexpected(-ENOEXEC);

        interp_is_relocatable = (interp_ehdr.e_type == ET_DYN);
    }

    page_table_req_ret_t pt_request = assign_page_table(task_id, 0, PAGE_TABLE_CREATE, ehdr.e_machine);
    if (pt_request.result)
        co_return std::unexpected((int)pt_request.result);
    pmos_pagetable_t page_table_id = pt_request.page_table;

    kernelLogger() << "processd: load_executable: assigned page table " << page_table_id << " for task " << task_id << "\n" << frg::endlog;
    co_return std::unexpected(-ENOSYS);
}

pmos::async::detached_task execve_handle(std::shared_ptr<Process> process, std::vector<std::byte> data, pmos::Right reply_right, pmos::Right fs_right, pmos::Right task_group_right)
{
    auto msg = reinterpret_cast<IPC_Execve *>(data.data());

    constexpr auto base_length = sizeof(*msg);
    if (!sum_less_or_equals(base_length, msg->path_length, data.size())) {
        execve_reply(std::move(reply_right), -EINVAL);
        co_return;
    }
    if (!sum_less_or_equals(base_length + msg->path_length, msg->args_length, data.size())) {
        execve_reply(std::move(reply_right), -EINVAL);
        co_return;
    }
    if (!sum_less_or_equals(base_length + msg->path_length + msg->args_length, msg->envs_length, data.size())) {
        execve_reply(std::move(reply_right), -EINVAL);
        co_return;
    }

    if (msg->path_length < 1) {
        execve_reply(std::move(reply_right), -EINVAL);
        co_return;
    }

    auto path = std::string(msg->data, strnlen(msg->data, msg->path_length - 1));
    std::vector<std::string> args;
    
    auto args_it = msg->data + msg->path_length;
    auto envs_it = args_it + msg->args_length;
    auto it = args_it;
    while (it < envs_it) {
        auto l = strnlen(it, envs_it - it);
        args.push_back(std::string(it, l));
        it += l + 1;
    }

    std::vector<std::string> envs;
    auto envs_end = envs_it + msg->envs_length;
    it = envs_it;
    while (it < envs_end) {
        auto l = strnlen(it, envs_end - it);
        envs.push_back(std::string(it, l));
        it += l + 1;
    }

    auto file_handle = co_await get_file_handle(path, process);
    if (!file_handle) {
        execve_reply(std::move(reply_right), file_handle.error());
        co_return;
    }

    auto new_task = syscall_new_task(PROCESS_RIGHT_NEW);
    if (new_task.result != SUCCESS) {
        execve_reply(std::move(reply_right), (int)new_task.result);
        co_return;
    }
    pmos::utility::scope_guard guard([&]{
        syscall_kill_task(new_task.value);
    });

    syscall_set_task_name(new_task.value, path.c_str(), path.size());

    auto exec_result = co_await load_executable(new_task.value, file_handle.value(), process);
    if (!exec_result) {
        execve_reply(std::move(reply_right), exec_result.error());
        co_return;
    }

    // TODO

    kernelLogger() << "processd: execve_handle: starting new task " << new_task.value << " for process " << process->pid << "\n" << frg::endlog;

    co_return;
}

pmos::async::detached_task handle_process_messages(pmos::ReceiveRight rr, std::shared_ptr<Process> process)
{
    while (1) {
        auto [msg, message, reply_right, extra_rights] = (co_await dispatcher.get_message(rr)).value();
    
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
            open_file(std::move(reply_right), path, process);
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
        
        case IPC_Openpt_NUM: {
            if (message.size() < sizeof(IPC_Openpt)) {
                kernelLogger() << "posixd: Received IPC_Openpt that is too small while attending file\n" << frg::endlog;
                break;
            }
            auto *openpt_msg = reinterpret_cast<IPC_Openpt *>(message.data());

            openpt_handle(std::move(reply_right), openpt_msg->flags);
        }
            break;
        case IPC_Register_Process_NUM: {
            if (msg.size < sizeof(IPC_Register_Process)) {
                kernelLogger() << "processd: Received IPC_Register_Process that is too small from task " << msg.sender << " of size " << msg.size << "\n" << frg::endlog;
                break;
            }

            IPC_Register_Process *m = reinterpret_cast<IPC_Register_Process *>(ipc_msg);
            register_process(m, std::move(reply_right), process, std::move(extra_rights[0]));
            break;
        }
        case IPC_Setsid_NUM: {
            if (msg.size < sizeof(IPC_Setsid)) {
                kernelLogger() << "processd: Received IPC_Setsid that is too small from task " << msg.sender << " of size " << msg.size << "\n" << frg::endlog;
                break;
            }

            // IPC_Setsid *m = reinterpret_cast<IPC_Setsid *>(ipc_msg);
            setsid_handle(process, std::move(reply_right));
            break;
        }
        case IPC_Execve_NUM: {
            if (msg.size < sizeof(IPC_Execve)) {
                kernelLogger() << "processd: Received IPC_Execve that is too small from task " << msg.sender << " (pid " << process->pid << ") of size " << msg.size << "\n" << frg::endlog;
                break;
            }

            execve_handle(process, std::move(message), std::move(reply_right), std::move(extra_rights[0]), std::move(extra_rights[1]));
            break;
        }

        default:
            kernelLogger() << "processd: Unknown message type " << ipc_msg->type << " from process\n" << frg::endlog;
            break;
        }
    }

    delete_process(process);
}

void register_process(IPC_Register_Process *msg, pmos::Right reply_right, std::shared_ptr<Process> process, pmos::Right process_right)
{
    (void)msg;
    (void)process;

    if (!process_right || process_right.type() != pmos::RightType::Process) {
        kernelLogger() << "processd: Received invalid process right for register_process\n" << frg::endlog;
        IPC_Register_Process_Reply reply = {
            .type = IPC_Register_Process_Reply_NUM,
            .flags = 0,
            .result = -EINVAL,
            .pid = 0,
        };

        auto r = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!r)
            kernelLogger() << "processd: Error " << r.error().first << " sending message to right " << reply_right.get() << " for register_process\n" << frg::endlog;
        return;
    }
    auto process_id = process_right.process_id();
    if (!process_id) {
        kernelLogger() << "processd: Received process right for register_process with no process id\n" << frg::endlog;
        IPC_Register_Process_Reply reply = {
            .type = IPC_Register_Process_Reply_NUM,
            .flags = 0,
            .result = -EINVAL,
            .pid = 0,
        };

        auto r = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!r)
            kernelLogger() << "processd: Error " << r.error().first << " sending message to right " << reply_right.get() << " for register_process\n" << frg::endlog;
        return;
    }
    if (get_process_kernel_id(process_id.value())) {
        kernelLogger() << "processd: Received process right for register_process with already registered process id " << process_id.value() << "\n" << frg::endlog;
        IPC_Register_Process_Reply reply = {
            .type = IPC_Register_Process_Reply_NUM,
            .flags = 0,
            .result = -EEXIST, // TODO!
            .pid = 0,
        };

        auto r = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!r)
            kernelLogger() << "processd: Error " << r.error().first << " sending message to right " << reply_right.get() << " for register_process\n" << frg::endlog;
        return;
    }

    auto new_process = create_process(process, std::move(process_right), process_id.value());

    auto right = main_port.create_right(pmos::RightType::SendMany);
    auto [send_right, receive_right] = std::move(right.value());
    handle_process_messages(std::move(receive_right), new_process);

    IPC_Register_Process_Reply reply = {
        .type = IPC_Register_Process_Reply_NUM,
        .flags = 0,
        .result = 0,
        .pid = new_process->pid,
    };

    auto r = pmos::send_message_right_one(reply_right, reply, {}, true, std::move(send_right));
    if (!r)
        kernelLogger() << "processd: Error " << r.error().first << " sending message to right " << reply_right.get() << " for register_process\n" << frg::endlog;
}

pmos::async::detached_task get_messages_bootstrapd(pmos::ReceiveRight rr)
{
    auto process = create_first_process();
    assert(process);

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
            register_process(m, std::move(reply_right), process, std::move(rights[0]));
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

    auto r = send_message_right_one(pr.value(), reply, {}, true, std::move(send_right));
    if (!r) {
        kernelLogger() << "processd: Error " << r.error().first << " sending message to right " << pr.value().get() << " for bootstrapd\n" << frg::endlog;
        return;
    }
}

int main(int argc, char *argv[])
{
    kernelLogger() << "processd started\n" << frg::endlog;
    parse_args(argc, argv);

    init_devfs();
    init_pty_filesystem();
    vfs_handle_messages();
    (void)dispatcher.dispatch();
    return 0;
}