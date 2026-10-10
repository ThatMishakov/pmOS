#pragma once
#include <array>
#include <cinttypes>
#include <cstdint>
#include <map>
#include <memory>
#include <unistd.h>
#include <pmos/helpers.hh>
#include <list>
#include <string>

struct Sigaction {
    uint64_t sa_handler_ = 0; // SIG_DFL
    uint64_t sa_restorer = 0;
    uint64_t sa_mask = 0;
    uint32_t sa_flags = 0;
};

int32_t allocate_pid();

struct ProcessGroup;
struct Session;
struct PtyData;
struct VNode;

struct Process {
    int32_t pid;
    std::shared_ptr<Process> parent;
    std::shared_ptr<ProcessGroup> process_group;

    std::map<int32_t, std::shared_ptr<Process>> children;
    std::list<std::shared_ptr<Process>> zombies;

    pmos::Right process_right;
    uint64_t kernel_process_id = 0;
    uint64_t receive_right_id = 0;
    bool running_exec = false;
    bool ran_exec = false;
    bool zombie = false;

    int exit_code = 0;

    uint32_t uid = 0;
    uint32_t euid = 0;
    uint32_t suid = 0;

    uint32_t gid = 0;
    uint32_t egid = 0;
    uint32_t sgid = 0;

    struct WaitpidRequest {
        pmos::Right reply_right;
        pid_t pid;
        int options;
    };

    std::list<WaitpidRequest> waitpid_requests;
    std::shared_ptr<VNode> cwd_vnode = nullptr;

    std::string get_cwd() const;
};

std::shared_ptr<Process> get_process_kernel_id(uint64_t kernel_process_id);

struct ProcessGroup {
    int32_t pgid = 0;
    std::map<int32_t, std::shared_ptr<Process>> processes;

    std::shared_ptr<Session> session;
};

struct Session {
    int32_t sid = 0;
    std::map<int32_t, std::shared_ptr<ProcessGroup>> process_groups;

    std::shared_ptr<PtyData> controlling_terminal = nullptr;
};

std::shared_ptr<Process> create_first_process();
std::shared_ptr<Process> create_process(std::shared_ptr<Process> parent, pmos::Right process_right, uint64_t kernel_process_id);
std::shared_ptr<Process> process_for_pid(int32_t pid);

void delete_process(std::shared_ptr<Process> process);
void setsid_handle(std::shared_ptr<Process> process, pmos::Right reply_right);
void setpgid_handle(std::shared_ptr<Process> process, pmos::Right reply_right, pid_t pid, pid_t pgid);
void waitpid_handle(std::shared_ptr<Process> process, pmos::Right reply_right, pid_t pid, int options);