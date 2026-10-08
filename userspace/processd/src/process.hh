#pragma once
#include <array>
#include <cinttypes>
#include <cstdint>
#include <map>
#include <memory>
#include <unistd.h>
#include <pmos/helpers.hh>

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

struct Process {
    int32_t pid;
    std::shared_ptr<Process> parent;
    std::shared_ptr<ProcessGroup> process_group;

    pmos::Right process_right;
    uint64_t kernel_process_id = 0;
    uint64_t receive_right_id = 0;
    bool running_exec = false;
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

void delete_process(std::shared_ptr<Process> process);
void setsid_handle(std::shared_ptr<Process> process, pmos::Right reply_right);