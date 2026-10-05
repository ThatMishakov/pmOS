#pragma once
#include <pmos/helpers.hh>
#include <string>
#include <memory>

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

    ~PtyData();
};