#include <pmos/async/coroutines.hh>
#include <pmos/containers/intrusive_list.hh>
#include <pmos/helpers.hh>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <inttypes.h>
#include <cassert>
#include "vfs.hh"
#include "log.hh"

extern pmos::Port main_port;
extern pmos::PortDispatcher dispatcher;

std::vector<std::shared_ptr<Filesystem>> filesystems;
std::shared_ptr<VNode> root_vnode;

void mount_filesystem_reply(pmos::Right &reply_right, int result)
{
    if (!reply_right)
        return;

    IPC_Mount_FS_Reply reply = {
        .type        = IPC_Mount_FS_Reply_NUM,
        .result_code = result,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!result_send)
        kernelLogger() << "vfsd: Error " << result_send.error() << " sending mount filesystem reply to port " << reply_right.get() << "\n" << frg::endlog;
}

struct RootNodeWaiter {
    void await_suspend(std::coroutine_handle<> h) noexcept;
    bool await_ready() noexcept;
    std::expected<std::shared_ptr<VNode>, int> await_resume() noexcept;

    std::coroutine_handle<> h_;
    pmos::containers::DoubleListHead<RootNodeWaiter> ll_head_;
};

bool RootNodeWaiter::await_ready() noexcept
{
    return static_cast<bool>(root_vnode);
}

std::expected<std::shared_ptr<VNode>, int> RootNodeWaiter::await_resume() noexcept
{
    return root_vnode;
}

using root_waiters_list = pmos::containers::CircularDoubleList<RootNodeWaiter, &RootNodeWaiter::ll_head_>;
root_waiters_list root_waiters;

void RootNodeWaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    h_ = std::move(h);
    root_waiters.push_back(this);
}

pmos::async::detached_task mount_filesystem(pmos::Right reply_right, pmos::Right fs_right, const std::string &mountpoint, int64_t root_inode)
{
    if (mountpoint != "/") {
        mount_filesystem_reply(reply_right, -ENOSYS);
        co_return;
    }

    if (!fs_right) {
        mount_filesystem_reply(reply_right, -EINVAL);
        co_return;
    }

    if (root_vnode) {
        mount_filesystem_reply(reply_right, -EEXIST);
        co_return;
    }

    auto fs = std::make_shared<Filesystem>();
    fs->fs_right = std::move(fs_right);
    fs->mountpoint = std::move(mountpoint);

    auto vnode = std::make_shared<VNode>();
    vnode->parent_fs = fs;
    vnode->inode = root_inode;
    vnode->type = FileType::Directory;

    fs->root = vnode;
    filesystems.push_back(fs);
    root_vnode = vnode;

    auto it = root_waiters.begin();
    while (it != root_waiters.end()) {
        root_waiters.remove(it);
        it->h_.resume();
        it = root_waiters.begin();
    }

    kernelLogger() << "vfsd: Mounted filesystem at " << fs->mountpoint << " with root inode " << root_inode << "\n" << frg::endlog;

    mount_filesystem_reply(reply_right, 0);
}

pmos::async::task<std::expected<std::shared_ptr<VNode>, int>> get_root_vnode()
{
    co_return co_await RootNodeWaiter{};
}

pmos::async::task<std::expected<std::shared_ptr<VNode>, int>> resolve_path(std::string path)
{
    auto p = Path::parse(path);

    // Start at root, since getcwd is not implemented

    auto root = co_await get_root_vnode();
    if (!root)
        co_return root;

    auto current_vnode = root.value();
    for (auto i : p.components()) {
        assert(!i.empty());
        if (!current_vnode->is_directory())
            co_return std::unexpected(-ENOTDIR);

        if (i == ".") {
            continue;
        } else if (i == "..") {
            if (current_vnode == root.value())
                continue;

            current_vnode = current_vnode->parent.lock();
            assert(current_vnode);
            continue;
        } else {
            auto p = co_await current_vnode->resolve_child(i);
            if (!p)
                co_return p;

            current_vnode = p.value();
        }
    }

    if (p.trailing_slash() && !current_vnode->is_directory())
        co_return std::unexpected(-ENOTDIR);

    co_return current_vnode;
}

pmos::async::task<std::expected<pmos::Right, int>> open_file_on_fs(std::shared_ptr<VNode> vnode)
{
    assert(vnode);

    IPC_FS_Open req = {
        .type  = IPC_FS_Open_NUM,
        .flags = 0,
        .inode = vnode->inode,
    };

    auto reply_right = pmos::send_message_right_one(vnode->parent_fs->fs_right, req, {&main_port, pmos::RightType::SendOnce});
    if (!reply_right) {
        kernelLogger() << "posixd: Error " << reply_right.error() << " sending open file message to filesystem\n" << frg::endlog;
        co_return std::unexpected(reply_right.error());
    }

    auto msg = co_await dispatcher.get_message(reply_right.value());
    if (!msg) {
        kernelLogger() << "posixd: Error " << msg.error() << " waiting for open file reply from filesystem\n" << frg::endlog;
        co_return std::unexpected(msg.error());
    }

    if (msg->descriptor.size < sizeof(IPC_FS_Open_Reply)) {
        kernelLogger() << "posixd: Invalid open file reply size " << msg->descriptor.size << "\n" << frg::endlog;
        co_return std::unexpected(-EIO);
    }

    auto *reply = reinterpret_cast<IPC_FS_Open_Reply *>(msg->data.data());
    if (reply->type != IPC_FS_Open_Reply_NUM) {
        kernelLogger() << "posixd: Invalid open file reply type " << reply->type << "\n" << frg::endlog;
        co_return std::unexpected(-EIO);
    }

    if (reply->result_code != 0) {
        kernelLogger() << "posixd: Filesystem returned error " << reply->result_code << " opening file\n" << frg::endlog;
        co_return std::unexpected(reply->result_code);
    }

    if (!msg->other_rights[0] || msg->other_rights[0].type() != pmos::RightType::SendMany) {
        kernelLogger() << "posixd: Invalid open file reply: missing file right\n" << frg::endlog;
        co_return std::unexpected(-EIO);
    }

    co_return std::move(msg->other_rights[0]);
}

void open_file_error_reply(pmos::Right &reply_right, int result)
{
    if (!reply_right)
        return;

    IPC_Open_Reply reply = {
        .type        = IPC_Open_Reply_NUM,
        .result_code = static_cast<int16_t>(result),
        .fs_flags    = 0,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error() << " sending open file reply to port " << reply_right.get() << "\n" << frg::endlog;
}

pmos::async::detached_task attend_open_file(std::shared_ptr<VNode> vnode, pmos::RecieveRight right)
{
    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message(right)).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posixd: Recieved very small message while attending file\n" << frg::endlog;
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Recieve_Right_Destroyed_NUM:
            break;
        default:
            kernelLogger() << "posixd: Unknown message type " << ipc_msg->type << " while attending file\n" << frg::endlog;
            break;
        }
    }

    co_return;
}

pmos::Right create_file_right(std::shared_ptr<VNode> vnode)
{
    auto [send_right, recieve_right] = main_port.create_right(pmos::RightType::SendMany).value();
    attend_open_file(std::move(vnode), std::move(recieve_right));
    return std::move(send_right);
}

pmos::async::detached_task open_file(pmos::Right reply_right, std::string path)
{
    auto result = co_await resolve_path(std::move(path));
    if (!result) {
        open_file_error_reply(reply_right, result.error());
        co_return;
    }

    auto vnode = result.value();
    if (vnode->type != FileType::File) {
        open_file_error_reply(reply_right, -EISDIR);
        co_return;
    }

    auto fs_right = co_await open_file_on_fs(vnode);
    if (!fs_right) {
        open_file_error_reply(reply_right, fs_right.error());
        co_return;
    }

    auto file_right = create_file_right(vnode);

    IPC_Open_Reply reply = {
        .type        = IPC_Open_Reply_NUM,
        .result_code = 0,
        .fs_flags    = 0,
    };

    auto send_result = pmos::send_message_right_one(reply_right, reply, {}, true, std::move(file_right), std::move(fs_right).value());
    if (!send_result)
        kernelLogger() << "posixd: Error " << send_result.error() << " sending open file reply to port " << reply_right.get() << "\n" << frg::endlog;
}

pmos::async::detached_task vfs_handle_messages()
{
    auto right = main_port.create_right(pmos::RightType::SendMany);
    auto [r, recieve_right] = std::move(right.value());
    auto result = pmos::name_right(std::move(r), "/pmos/vfsd");

    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message_default()).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posixd: Warning: recieved very small message\n" << frg::endlog;
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Mount_FS_NUM: {
            if (message.size() < sizeof(IPC_Mount_FS)) {
                kernelLogger() << "posixd: Recieved IPC_Mount_FS that is too small from task " << msg.sender << " of size " << message.size() << "\n" << frg::endlog;
                break;
            }

            auto *m = reinterpret_cast<IPC_Mount_FS *>(message.data());
            std::string mountpoint(m->mount_path, message.size() - sizeof(IPC_Mount_FS));
            mount_filesystem(std::move(reply_right), std::move(rights[0]), mountpoint, m->root_fd);
        } break;
        case IPC_Open_NUM: {
            if (message.size() < sizeof(IPC_Open)) {
                kernelLogger() << "posixd: Recieved IPC_Open that is too small from task " << msg.sender << " of size " << message.size() << "\n" << frg::endlog;
                break;
            }

            auto *m = reinterpret_cast<IPC_Open *>(message.data());
            std::string path(m->path, message.size() - sizeof(IPC_Open));
            open_file(std::move(reply_right), path);
        } break;
        default:
            kernelLogger() << "posixd: Unknown message type " << ipc_msg->type << "\n" << frg::endlog;
            break;
        }
    }

    co_return;
}

Path Path::parse(const std::string &path)
{
    Path result {};

    auto it = path.begin();
    if (it != path.end() && *it == '/') {
        result._relative = false;
        ++it;
    } else
        result._relative = true;

    if (!path.empty() && path.back() == '/')
        result._trailing_slash = true;

    while (it != path.end()) {
        auto start = it;
        it = std::find(it, path.end(), '/');

        if (start != it)
            result._components.emplace_back(start, it);

        if (it != path.end())
            ++it;
    }

    return result;
}

void unblock_vnode_waiters(std::shared_ptr<VNode> vnode, const std::string &name, std::expected<std::shared_ptr<VNode>, int> result)
{
    auto it = vnode->children_cache.find(name);
    if (it == vnode->children_cache.end())
        return;

    if (const auto ptr = std::get_if<VNode::vnode_ptr>(&it->second); ptr) {
        assert(*ptr == result.value_or(nullptr));
        return;
    }

    assert(std::holds_alternative<VNodeAwaitersList>(it->second));

    auto &waiters = std::get<VNodeAwaitersList>(it->second);
    auto waiter_it = waiters.begin();
    while (waiter_it != waiters.end()) {
        waiters.remove(waiter_it);
        waiter_it->result_ = result;
        waiter_it->h_.resume();
        waiter_it = waiters.begin();
    }

    if (result)
        it->second = result.value();
    else
        vnode->children_cache.erase(it);
}

bool VNodeAwaiter::await_ready() noexcept
{
    auto it = vnode_->children_cache.find(name_);
    if (it == vnode_->children_cache.end())
        return true;

    if (const auto ptr = std::get_if<std::shared_ptr<VNode>>(&it->second); ptr) {
        result_ = *ptr;
        return true;
    }

    return false;
}

void VNodeAwaiter::await_suspend(std::coroutine_handle<> h)
{
    h_ = std::move(h);

    auto &waiters = std::get<VNodeAwaitersList>(vnode_->children_cache[name_]);
    waiters.push_back(this);
}

std::expected<std::shared_ptr<VNode>, int> VNodeAwaiter::await_resume() noexcept
{
    return std::move(result_);
}

pmos::async::detached_task vnode_wait(std::shared_ptr<VNode> vnode, std::string name, pmos::RecieveRight right)
{
    auto msg = co_await dispatcher.get_message(right);
    if (!msg) {
        kernelLogger() << "posixd: Failed to get a reply from the failsystem\n" << frg::endlog;

        unblock_vnode_waiters(vnode, name, std::unexpected(-EIO));
        co_return;
    }

    if (msg->descriptor.size < sizeof(IPC_FS_Resolve_Path_Reply)) {
        kernelLogger() << "posixd: Invalid resolve child reply size " << msg->descriptor.size << "\n" << frg::endlog;
        unblock_vnode_waiters(vnode, name, std::unexpected(-EIO));
        co_return;
    }

    auto *reply = reinterpret_cast<IPC_FS_Resolve_Path_Reply *>(msg->data.data());
    if (reply->type != IPC_FS_Resolve_Path_Reply_NUM) {
        kernelLogger() << "posixd: Invalid resolve child reply type " << reply->type << "\n" << frg::endlog;
        unblock_vnode_waiters(vnode, name, std::unexpected(-EIO));
        co_return;
    }

    if (reply->result_code != 0) {
        unblock_vnode_waiters(vnode, name, std::unexpected(reply->result_code));
        co_return;
    }

    auto child_vnode = std::make_shared<VNode>();
    child_vnode->parent_fs = vnode->parent_fs;
    child_vnode->parent = vnode;
    child_vnode->inode = reply->file_id;
    child_vnode->type = file_type_from_ipc(reply->file_type);

    unblock_vnode_waiters(vnode, name, child_vnode);
}

pmos::async::task<std::expected<std::shared_ptr<VNode>, int>> VNode::resolve_child(const std::string &name)
{
    assert(!name.empty());
    auto it = children_cache.find(name);
    if (it != children_cache.end()) {
        if (const auto ptr = std::get_if<vnode_ptr>(&it->second); ptr) {
            if (*ptr)
                co_return *ptr;

            co_return std::unexpected(-ENOENT);
        }

        assert(std::holds_alternative<VNodeAwaitersList>(it->second));

        co_return co_await VNodeAwaiter(shared_from_this(), name);
    }

    std::vector<uint8_t> buffer(sizeof(IPC_FS_Resolve_Path) + name.size());
    auto req = reinterpret_cast<IPC_FS_Resolve_Path *>(buffer.data());
    req->type = IPC_FS_Resolve_Path_NUM;
    req->flags = 0;
    req->inode = inode;
    memcpy(req->path_name, name.data(), name.size());

    auto span = std::span<const uint8_t>(buffer.data(), sizeof(IPC_FS_Resolve_Path) + name.size());

    auto send_result = pmos::send_message_right(parent_fs->fs_right, span, {&main_port, pmos::RightType::SendOnce}, false);
    if (!send_result) {
        kernelLogger() << "posixd: Error " << send_result.error() << " sending resolve child message to filesystem\n" << frg::endlog;
        co_return std::unexpected(send_result.error());
    }

    children_cache[name] = VNodeAwaitersList{};

    vnode_wait(shared_from_this(), name, std::move(send_result.value()));

    co_return co_await VNodeAwaiter(shared_from_this(), name);
}

FileType file_type_from_ipc(uint32_t ipc_file_type)
{
    FileType result = FileType::None;

    switch (ipc_file_type) {
    case IPC_FILE_TYPE_REGULAR:
        result = FileType::File;
        break;
    case IPC_FILE_TYPE_DIRECTORY:
        result = FileType::Directory;
        break;
    default:
        // TODO!
        break;
    }

    return result;
}