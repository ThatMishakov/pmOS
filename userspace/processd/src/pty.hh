#pragma once
#include <pmos/helpers.hh>
#include <string>
#include <memory>
#include <deque>
#include <termios.h>

void init_pty_filesystem();

void openpt_handle(pmos::Right reply_right, unsigned oflags);

struct VNode;
struct Session;

struct PtyData {
    bool have_manager = true;
    bool locked = true;

    size_t subordinate_count = 0;

    unsigned idx = 0;

    std::string name() const {
        return std::to_string(idx);
    }

    std::string path();

    std::shared_ptr<VNode> vnode = nullptr;
    std::shared_ptr<Session> session = nullptr;

    struct Packet {
        std::vector<char> data;

        size_t offset = 0;
    };

    struct termios active_settings = {};

    std::deque<Packet> manager_queue;
    std::deque<Packet> subordinate_queue;

    ~PtyData();
};