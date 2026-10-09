#include "process.hh"
#include <memory>
#include <cassert>
#include <pmos/ipc.h>
#include "log.hh"
#include <unordered_map>

int32_t allocate_pid()
{
    // TODO: Implement PID recycling and stuff
    static int32_t next_pid = 2;
    return next_pid++;
}

std::map<int32_t, std::shared_ptr<Session>> sessions;
std::map<int32_t, std::shared_ptr<ProcessGroup>> process_groups;
std::map<int32_t, std::shared_ptr<Process>> processes;

std::unordered_map<uint64_t, std::shared_ptr<Process>> processes_by_kernel_id;

std::shared_ptr<Session> create_session(pid_t sid)
{
    auto session = std::make_shared<Session>();
    session->sid = sid;
    sessions[sid] = session;
    return session;
}

std::shared_ptr<ProcessGroup> create_process_group(pid_t pgid, std::shared_ptr<Session> session)
{
    if (!session) {
        session = create_session(pgid);
    }

    auto process_group = std::make_shared<ProcessGroup>();
    process_group->pgid = pgid;
    process_group->session = session;
    process_groups[pgid] = process_group;
    session->process_groups[pgid] = process_group;
    return process_group;
}

std::shared_ptr<Process> create_first_process()
{
    auto pid = 1;

    auto process = std::make_shared<Process>();
    process->pid = pid;
    process->process_group = create_process_group(process->pid, nullptr);
    process->process_group->processes[process->pid] = process;
    processes[process->pid] = process;
    return process;
}

std::shared_ptr<Process> create_process(std::shared_ptr<Process> parent, pmos::Right process_right, uint64_t kernel_process_id)
{
    assert(parent);
    auto process = std::make_shared<Process>();
    process->pid = allocate_pid();
    process->parent = parent;
    process->process_group = parent->process_group;
    process->process_right = std::move(process_right);
    processes[process->pid] = process;
    parent->process_group->processes[process->pid] = process;
    processes_by_kernel_id[kernel_process_id] = process;

    process->uid = parent->uid;
    process->gid = parent->gid;
    process->euid = parent->euid;
    process->egid = parent->egid;

    return process;
}

std::shared_ptr<Process> process_for_pid(int32_t pid)
{
    auto it = processes.find(pid);
    if (it == processes.end())
        return nullptr;
    return it->second;
}

void remove_process_group_from_session(std::shared_ptr<Session> session, std::shared_ptr<ProcessGroup> group)
{
    assert(session);
    assert(group);

    session->process_groups.erase(group->pgid);
    if (session->process_groups.empty()) {
        sessions.erase(session->sid);
        // TODO
    }
}

void remove_process_from_group(std::shared_ptr<ProcessGroup> group, std::shared_ptr<Process> process)
{
    assert(process);
    assert(group);

    group->processes.erase(process->pid);
    if (group->processes.empty()) {
        remove_process_group_from_session(group->session, group);
        process_groups.erase(group->pgid);
        // TODO
    }
}

void delete_process(std::shared_ptr<Process> process)
{
    if (!process)
        return;

    remove_process_from_group(process->process_group, process);
    processes.erase(process->pid);
    processes_by_kernel_id.erase(process->kernel_process_id);
    // TODO
}

std::shared_ptr<Process> get_process_kernel_id(uint64_t kernel_process_id)
{
    auto it = processes_by_kernel_id.find(kernel_process_id);
    if (it == processes_by_kernel_id.end())
        return nullptr;
    return it->second;
}

void setsid_reply(pmos::Right &reply_right, int32_t result_sid)
{
    if (!reply_right)
        return;

    IPC_Setsid_Reply reply = {
        .type = IPC_Setsid_Reply_NUM,
        .flags = 0,
        .result_sid = result_sid,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!result_send)
        kernelLogger() << "processd: Error " << result_send.error().first << " sending setsid reply to port " << reply_right.get() << "\n" << frg::endlog;
}

void setsid_handle(std::shared_ptr<Process> process, pmos::Right reply_right)
{
    assert(process);
    assert(process->process_group);

    if (process->process_group->pgid == process->pid) {
        setsid_reply(reply_right, -EPERM);
        return;
    }
    if (process_groups.find(process->pid) != process_groups.end()) {
        setsid_reply(reply_right, -EPERM);
        return;
    }

    auto new_group = create_process_group(process->pid, nullptr);
    remove_process_from_group(process->process_group, process);
    process->process_group = new_group;
    new_group->processes[process->pid] = process;

    setsid_reply(reply_right, new_group->pgid);
}