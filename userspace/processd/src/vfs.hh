#pragma once
#include <vector>
#include <string>
#include <pmos/async/coroutines.hh>
#include <cassert>
#include <variant>

struct VNode;

struct Filesystem {
    pmos::Right fs_right;
    std::string mountpoint;

    std::shared_ptr<VNode> root;
};

enum class FileType {
    None,
    File,
    Directory,
    // TODO
};

FileType file_type_from_ipc(uint32_t ipc_file_type);

struct VNodeAwaiter {
    void await_suspend(std::coroutine_handle<> h);
    bool await_ready() noexcept;
    std::expected<std::shared_ptr<VNode>, int> await_resume() noexcept;

    inline VNodeAwaiter(std::shared_ptr<VNode> vnode, std::string name):
        vnode_(std::move(vnode)), name_(std::move(name))
    {
        assert(vnode_);
        assert(!name_.empty());
    }

    std::coroutine_handle<> h_;
    pmos::containers::DoubleListHead<VNodeAwaiter> ll_head_;
    std::shared_ptr<VNode> vnode_;
    std::string name_;
    std::expected<std::shared_ptr<VNode>, int> result_;
};

using VNodeAwaitersList = pmos::containers::CircularDoubleList<VNodeAwaiter, &VNodeAwaiter::ll_head_>;

struct VNode: public std::enable_shared_from_this<VNode> {
    std::shared_ptr<Filesystem> parent_fs;
    std::weak_ptr<VNode> parent;
    uint64_t inode = 0;
    std::string name;

    FileType type = FileType::None;

    using vnode_ptr = std::shared_ptr<VNode>;

    std::map<std::string, std::variant<vnode_ptr, VNodeAwaitersList>> children_cache;

    inline bool is_directory() const
    {
        return type == FileType::Directory;
    }

    pmos::async::task<std::expected<std::shared_ptr<VNode>, int>> resolve_child(const std::string &name);
};

extern std::vector<std::shared_ptr<Filesystem>> filesystems;
extern std::shared_ptr<VNode> root_vnode;


struct Path {
    static Path parse(const std::string &path);

    inline bool relative() const
    {
        return _relative;
    }
    inline bool trailing_slash() const
    {
        return _trailing_slash;
    }
    inline const std::vector<std::string> &components() const
    {
        return _components;
    }


    bool _relative = false;
    bool _trailing_slash = false;
    std::vector<std::string> _components;
};