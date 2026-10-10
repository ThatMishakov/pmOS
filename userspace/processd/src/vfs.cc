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
#include <fcntl.h>
#include "devfs.hh"
#include <pmos/fs-data.h>

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
        kernelLogger() << "vfsd: Error " << result_send.error().first << " sending mount filesystem reply to port " << reply_right.get() << "\n" << frg::endlog;
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

pmos::async::task<std::expected<std::shared_ptr<VNode>, int>> get_root_vnode()
{
    co_return co_await RootNodeWaiter{};
}

pmos::async::task<std::expected<std::string, int>> read_symlink(std::shared_ptr<VNode> vnode)
{
    assert(vnode);
    assert(vnode->is_link());

    auto fs = vnode->parent_fs;
    assert(fs);

    auto result = co_await fs->read_symlink(vnode);
    co_return result;
}

pmos::async::task<std::expected<std::shared_ptr<VNode>, int>> resolve_path(std::string path, std::shared_ptr<VNode> current_vnode = nullptr, std::shared_ptr<VNode> root_vnode = nullptr, bool follow_symlinks = true)
{
    int symlink_count = 0;

    while (symlink_count < 40) { // SYMLOOP_MAX
        ++symlink_count;

        auto p = Path::parse(path);

        // Start at root, since getcwd is not implemented
        if (!root_vnode) {
            auto root = co_await get_root_vnode();
            if (!root)
                co_return root;
            root_vnode = std::move(root.value());
        }


        if (!current_vnode || !p.relative()) {
            current_vnode = root_vnode;
        }

        for (auto i : p.components()) {
            assert(!i.empty());
            if (!current_vnode->is_directory())
                co_return std::unexpected(-ENOTDIR);

            if (i == ".") {
                continue;
            } else if (i == "..") {
                if (current_vnode == root_vnode)
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

        if (!follow_symlinks || !current_vnode->is_link())
            co_return current_vnode;

        auto symlink_path = co_await read_symlink(current_vnode);
        if (!symlink_path)
            co_return std::unexpected(symlink_path.error());

        path = std::move(symlink_path.value());
        current_vnode = current_vnode->parent.lock();
    }

    co_return std::unexpected(-ELOOP);
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

    auto fs = std::make_shared<ExternalFilesystem>();
    fs->fs_right = std::move(fs_right);

    auto vnode = std::make_shared<VNode>();
    vnode->parent_fs = fs;
    vnode->inode = root_inode;
    vnode->type = FileType::Directory;

    fs->root = vnode;

    std::shared_ptr<VNode> new_root = nullptr;
    if (root_vnode) {
        // Only pivot root for now
        // TODO: This should be extended to mounting at arbitrary mountpoints and should be trivial

        auto name = "/run/initramfs";
        auto n = co_await resolve_path(name, nullptr, vnode);
        if (!n) {
            mount_filesystem_reply(reply_right, n.error());
            co_return;
        }
        new_root = std::move(n.value());

        if (new_root->type != FileType::Directory) {
            mount_filesystem_reply(reply_right, -ENOTDIR);
            co_return;
        }

        auto mount_parent = new_root->parent.lock();
        assert(mount_parent);

        root_vnode->parent = mount_parent;
        root_vnode->name = new_root->name;
        mount_parent->children_cache[new_root->name] = root_vnode;
        new_root = std::move(root_vnode);
        root_vnode = vnode;
    } else {
        root_vnode = vnode;
    }

    filesystems.push_back(fs);

    assert(devfs_root_vnode);
    auto devfs_parent = devfs_root_vnode->parent.lock();
    if (devfs_parent) {
        devfs_parent->children_cache.erase(devfs_root_vnode->name);
    }

    devfs_root_vnode->parent = root_vnode;
    root_vnode->children_cache[devfs_root_vnode->name] = devfs_root_vnode;



    auto it = root_waiters.begin();
    while (it != root_waiters.end()) {
        root_waiters.remove(it);
        it->h_.resume();
        it = root_waiters.begin();
    }

    kernelLogger() << "vfsd: Mounted filesystem at " << fs->root->path() << " with root inode " << root_inode << "\n" << frg::endlog;
    if (new_root)
        kernelLogger() << "vfsd: Pivoted old root to " << new_root->path() << "\n" << frg::endlog;

    mount_filesystem_reply(reply_right, 0);
}

pmos::async::task<std::expected<pmos::Right, int>> ExternalFilesystem::open_file(std::shared_ptr<VNode> vnode, std::shared_ptr<Process>)
{
    assert(vnode);
    assert(vnode->parent_fs.get() == this);

    IPC_FS_Open req = {
        .type  = IPC_FS_Open_NUM,
        .flags = 0,
        .inode = vnode->inode,
    };

    auto reply_right = pmos::send_message_right_one(fs_right, req, {&main_port, pmos::RightType::SendOnce});
    if (!reply_right) {
        kernelLogger() << "posixd: Error " << reply_right.error().first << " sending open file message to filesystem\n" << frg::endlog;
        co_return std::unexpected(reply_right.error().first);
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
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending open file reply to port " << reply_right.get() << "\n" << frg::endlog;
}

void handle_ttyname(std::shared_ptr<VNode> vnode, pmos::Right reply_right)
{
    if (!reply_right)
        return;

    if (!vnode->is_tty) {
        IPC_Ttyname_Reply reply = {
            .type        = IPC_Ttyname_Reply_NUM,
            .result_code = -ENOTTY,
            .flags       = 0,
            .tty_name    = {},
        };

        auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!result_send)
            kernelLogger() << "posixd: Error " << result_send.error().first << " sending ttyname reply to port " << reply_right.get() << "\n" << frg::endlog;
        return;
    }

    auto path = vnode->path();
    size_t path_length = path.size();

    size_t reply_size = sizeof(IPC_Ttyname_Reply) + path_length;
    std::unique_ptr<char[]> reply_data(new char[reply_size]);
    auto *reply = reinterpret_cast<IPC_Ttyname_Reply *>(reply_data.get());
    reply->type = IPC_Ttyname_Reply_NUM;
    reply->result_code = 0;
    reply->flags = 0;
    memcpy(reply->tty_name, path.c_str(), path_length);

    auto span = std::span(reply_data.get(), reply_size);
    auto result_send = pmos::send_message_right(
        reply_right, span, std::pair<pmos::Port const *, pmos::RightType>{nullptr, pmos::RightType::SendOnce}, true);
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending ttyname reply to port " << reply_right.get() << "\n" << frg::endlog;
}

pmos::async::detached_task attend_open_file(std::shared_ptr<VNode> vnode, pmos::ReceiveRight right)
{
    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message(right)).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posixd: Received very small message while attending file\n" << frg::endlog;
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            break;
        case IPC_Stat_NUM: {
            if (message.size() < sizeof(IPC_Stat)) {
                kernelLogger() << "posixd: Received IPC_Stat that is too small while attending file\n" << frg::endlog;
                break;
            }
            auto *stat_msg = reinterpret_cast<IPC_Stat *>(message.data());

            std::string stat_msg_path(stat_msg->path, message.size() - sizeof(IPC_Stat));

            stat_handle(vnode, std::move(reply_right), stat_msg->flags, std::move(stat_msg_path));
        }
            break;
        case IPC_Ttyname_NUM: {
            if (message.size() < sizeof(IPC_Ttyname)) {
                kernelLogger() << "posixd: Received IPC_Ttyname that is too small while attending pty\n" << frg::endlog;
                break;
            }

            handle_ttyname(vnode, std::move(reply_right));
        }
            break;
        case IPC_Unlockpt_NUM: {
            if (message.size() < sizeof(IPC_Unlockpt)) {
                kernelLogger() << "posixd: Received IPC_Unlockpt that is too small while attending pty\n" << frg::endlog;
                break;
            }

            auto result = co_await vnode->parent_fs->unlockpt(vnode);

            IPC_Unlockpt_Reply reply = {
                .type        = IPC_Unlockpt_Reply_NUM,
                .result_code = static_cast<int16_t>(result.error_or(0)),
                .flags       = 0,
            };

            auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
            if (!result_send)
                kernelLogger() << "posixd: Error " << result_send.error().first << " sending unlockpt reply to port " << reply_right.get() << "\n" << frg::endlog;
        } break;
        default:
            kernelLogger() << "posixd: Unknown message type " << ipc_msg->type << " while attending file\n" << frg::endlog;
            break;
        }
    }

    co_return;
}

pmos::Right create_file_right(std::shared_ptr<VNode> vnode)
{
    auto [send_right, receive_right] = main_port.create_right(pmos::RightType::SendMany).value();
    attend_open_file(std::move(vnode), std::move(receive_right));
    return std::move(send_right);
}

pmos::async::task<std::expected<pmos::Right /* io_right */, int>> get_file_handle(std::string path, std::shared_ptr<Process> process)
{
    auto result = co_await resolve_path(std::move(path));
    if (!result) {
        co_return std::unexpected(result.error());
    }

    auto vnode = result.value();
    if (vnode->type == FileType::Directory) {
        co_return std::unexpected(EISDIR);
    }

    co_return co_await vnode->parent_fs->open_file(vnode, process);    
}

pmos::async::detached_task open_file(pmos::Right reply_right, std::string path, std::shared_ptr<Process> process)
{
    auto result = co_await resolve_path(std::move(path));
    if (!result) {
        open_file_error_reply(reply_right, result.error());
        co_return;
    }

    auto vnode = result.value();
    if (vnode->type == FileType::Directory) {
        open_file_error_reply(reply_right, -EISDIR);
        co_return;
    }

    auto fs_right = co_await vnode->parent_fs->open_file(vnode, process);
    if (!fs_right) {
        open_file_error_reply(reply_right, fs_right.error());
        co_return;
    }

    auto file_right = create_file_right(vnode);

    uint16_t flags = 0;
    if (vnode->is_tty)
        flags |= FLAG_ISATTY;

    IPC_Open_Reply reply = {
        .type        = IPC_Open_Reply_NUM,
        .result_code = 0,
        .fs_flags    = flags,
    };

    auto send_result = pmos::send_message_right_one(reply_right, reply, {}, true, std::move(file_right), std::move(fs_right).value());
    if (!send_result)
        kernelLogger() << "posixd: Error " << send_result.error().first << " sending open file reply to port " << reply_right.get() << "\n" << frg::endlog;
}

pmos::async::task<std::expected<StatData, int>> ExternalFilesystem::get_file_stat_dynamic(std::shared_ptr<VNode> vnode)
{
    assert(vnode);
    assert(vnode->parent_fs.get() == this);
    
    IPC_FS_Stat_Dynamic req = {
        .type  = IPC_FS_Stat_Dynamic_NUM,
        .flags = 0,
        .inode = vnode->inode,
    };

    auto reply_right = pmos::send_message_right_one(fs_right, req, {&main_port, pmos::RightType::SendOnce});
    if (!reply_right) {
        kernelLogger() << "posixd: Error " << reply_right.error().first << " sending stat dynamic message to filesystem\n" << frg::endlog;
        co_return std::unexpected(reply_right.error().first);
    }

    auto msg = co_await dispatcher.get_message(reply_right.value());
    if (!msg) {
        kernelLogger() << "posixd: Error " << msg.error() << " waiting for stat dynamic reply from filesystem\n" << frg::endlog;
        co_return std::unexpected(msg.error());
    }

    if (msg->descriptor.size < sizeof(IPC_FS_Stat_Dynamic_Reply)) {
        kernelLogger() << "posixd: Invalid stat dynamic reply size " << msg->descriptor.size << "\n" << frg::endlog;
        co_return std::unexpected(-EIO);
    }

    auto *reply = reinterpret_cast<IPC_FS_Stat_Dynamic_Reply *>(msg->data.data());
    if (reply->type != IPC_FS_Stat_Dynamic_Reply_NUM) {
        kernelLogger() << "posixd: Invalid stat dynamic reply type " << reply->type << "\n" << frg::endlog;
        co_return std::unexpected(-EIO);
    }

    if (reply->result_code != 0) {
        kernelLogger() << "posixd: Filesystem returned error " << reply->result_code << " getting stat dynamic\n" << frg::endlog;
        co_return std::unexpected(reply->result_code);
    }

    co_return StatData {
        .st_size = reply->st_size,
        .st_nlink = reply->st_nlink,
        .st_atim_tv_nsec = reply->st_atim_tv_nsec,
        .st_mtim_tv_nsec = reply->st_mtim_tv_nsec,
        .st_ctim_tv_nsec = reply->st_ctim_tv_nsec,
        .st_btim_tv_nsec = reply->st_btim_tv_nsec,
        .st_blocks = reply->st_blocks,
    };
}


void stat_handle_error_reply(pmos::Right &reply_right, int result)
{
    if (!reply_right)
        return;

    IPC_Stat_Reply reply = {
        .type = IPC_Stat_Reply_NUM,
        .flags = 0,
        .result = static_cast<int16_t>(result),
        .st_dev = 0,
        .st_ino = 0,
        .st_mode = 0,
        .st_nlink = 0,
        .st_uid = 0,
        .st_gid = 0,
        .st_rdev = 0,
        .st_size = 0,
        .st_atim_tv_nsec = 0,
        .st_mtim_tv_nsec = 0,
        .st_ctim_tv_nsec = 0,
        .st_blksize = 0,
        .st_blocks = 0,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending stat reply to port " << reply_right.get() << "\n" << frg::endlog;
}

pmos::async::detached_task stat_handle(std::shared_ptr<VNode> vnode, pmos::Right reply_right, unsigned flags, std::string path)
{
    bool empty_path = flags & AT_EMPTY_PATH;

    // TODO: Handle symlinks here, and AT_SYMLINK_NOFOLLOW
    if (!vnode && empty_path) {
        stat_handle_error_reply(reply_right, -EINVAL);
        co_return;
    }

    auto result = co_await resolve_path(std::move(path), std::move(vnode));
    if (!result) {
        stat_handle_error_reply(reply_right, result.error());
        co_return;
    }

    auto vnode_resolved = result.value();

    auto stat_result = co_await vnode_resolved->parent_fs->get_file_stat_dynamic(vnode_resolved);
    if (!stat_result) {
        stat_handle_error_reply(reply_right, stat_result.error());
        co_return;
    }

    auto stat_data = stat_result.value();

    IPC_Stat_Reply reply = {
        .type = IPC_Stat_Reply_NUM,
        .flags = 0,
        .result = 0,
        .st_dev = vnode_resolved->parent_fs->device_id,
        .st_ino = vnode_resolved->inode,
        .st_mode = vnode_resolved->st_mode,
        .st_nlink = stat_data.st_nlink,
        .st_uid = vnode_resolved->st_uid,
        .st_gid = vnode_resolved->st_gid,
        .st_rdev = vnode_resolved->st_rdev,
        .st_size = stat_data.st_size,
        .st_atim_tv_nsec = stat_data.st_atim_tv_nsec,
        .st_mtim_tv_nsec = stat_data.st_mtim_tv_nsec,
        .st_ctim_tv_nsec = stat_data.st_ctim_tv_nsec,
        .st_blksize = vnode_resolved->st_blksize,
        .st_blocks = stat_data.st_blocks,
    };

    auto send_result = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!send_result)
        kernelLogger() << "posixd: Error " << send_result.error().first << " sending stat reply to port " << reply_right.get() << "\n" << frg::endlog;
}

pmos::async::detached_task vfs_handle_messages()
{
    auto right = main_port.create_right(pmos::RightType::SendMany);
    auto [r, receive_right] = std::move(right.value());
    auto result = co_await pmos::name_right(dispatcher, std::move(r), "/pmos/vfsd");
    result.value();

    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message_default()).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posixd: Warning: received very small message\n" << frg::endlog;
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Mount_FS_NUM: {
            if (message.size() < sizeof(IPC_Mount_FS)) {
                kernelLogger() << "posixd: Received IPC_Mount_FS that is too small from task " << msg.sender << " of size " << message.size() << "\n" << frg::endlog;
                break;
            }

            auto *m = reinterpret_cast<IPC_Mount_FS *>(message.data());
            std::string mountpoint(m->mount_path, message.size() - sizeof(IPC_Mount_FS));
            mount_filesystem(std::move(reply_right), std::move(rights[0]), mountpoint, m->root_fd);
        } break;
        // case IPC_Open_NUM: {
        //     if (message.size() < sizeof(IPC_Open)) {
        //         kernelLogger() << "posixd: Received IPC_Open that is too small from task " << msg.sender << " of size " << message.size() << "\n" << frg::endlog;
        //         break;
        //     }

        //     auto *m = reinterpret_cast<IPC_Open *>(message.data());
        //     std::string path(m->path, message.size() - sizeof(IPC_Open));
        //     open_file(std::move(reply_right), path);
        // } break;
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

    auto waiters = std::move(std::get<VNodeAwaitersList>(it->second));
    if (result)
        it->second = result.value();
    else
        vnode->children_cache.erase(it);

    while (!waiters.empty()) {
        auto waiter = &waiters.front();
        waiters.remove(waiter);
        waiter->result_ = result;
        waiter->h_.resume();
    }
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

pmos::async::detached_task vnode_wait(std::shared_ptr<VNode> vnode, std::string name, pmos::ReceiveRight right)
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
    child_vnode->name = name;
    child_vnode->inode = reply->file_id;
    child_vnode->type = file_type_from_ipc(reply->file_type);

    child_vnode->st_mode = reply->st_mode;
    child_vnode->st_blksize = reply->st_blksize;
    child_vnode->st_uid = reply->st_uid;
    child_vnode->st_gid = reply->st_gid;
    child_vnode->st_rdev = reply->st_rdev;

    unblock_vnode_waiters(vnode, name, child_vnode);
}

std::expected<std::shared_ptr<VNode>, int> ExternalFilesystem::resolve_child(std::shared_ptr<VNode> parent, const std::string &name)
{
    std::vector<uint8_t> buffer(sizeof(IPC_FS_Resolve_Path) + name.size());
    auto req = reinterpret_cast<IPC_FS_Resolve_Path *>(buffer.data());
    req->type = IPC_FS_Resolve_Path_NUM;
    req->flags = 0;
    req->inode = parent->inode;
    memcpy(req->path_name, name.data(), name.size());

    auto span = std::span<const uint8_t>(buffer.data(), sizeof(IPC_FS_Resolve_Path) + name.size());

    auto send_result = pmos::send_message_right(fs_right, span, {&main_port, pmos::RightType::SendOnce}, false);
    if (!send_result) {
        kernelLogger() << "posixd: Error " << send_result.error().first << " sending resolve child message to filesystem\n" << frg::endlog;
        return std::unexpected(send_result.error().first);
    }

    parent->children_cache[name] = VNodeAwaitersList{};

    vnode_wait(parent, name, std::move(send_result.value()));

    return {};
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

    assert(parent_fs);
    auto val = parent_fs->resolve_child(shared_from_this(), name);

    if (val) {
        auto value = std::move(val.value());
        if (value)
            co_return value;
        else
            co_return co_await VNodeAwaiter(shared_from_this(), name);
    } else {
        co_return std::unexpected(val.error());
    }
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
    case IPC_FILE_TYPE_CHAR:
        result = FileType::CharacterDevice;
        break;
    case IPC_FILE_TYPE_LINK:
        result = FileType::Symlink;
        break;
    default:
        // TODO!
        break;
    }

    return result;
}

std::string VNode::path() const
{
    std::string result;
    const VNode *current = this;

    size_t max_iterations = 1000; // Prevent infinite loops in case of cycles

    while (current) {
        if (!current->name.empty())
            result = "/" + current->name + result;

        auto parent_vnode = current->parent.lock();
        current = parent_vnode.get();

        if (--max_iterations == 0) {
            kernelLogger() << "vfsd: Warning: Detected potential cycle in VNode parent chain while computing path\n" << frg::endlog;
            break;
        }
    }
    return result.empty() ? "/" : result;
}

pmos::async::task<std::expected<std::string, int>> ExternalFilesystem::read_symlink(std::shared_ptr<VNode> vnode)
{
    IPC_FS_Readlink req = {
        .type  = IPC_FS_Readlink_NUM,
        .flags = 0,
        .inode = vnode->inode,
    };

    auto reply_right = pmos::send_message_right_one(fs_right, req, {&main_port, pmos::RightType::SendOnce});
    if (!reply_right) {
        kernelLogger() << "posixd: Error " << reply_right.error().first << " sending read symlink message to filesystem\n" << frg::endlog;
        co_return std::unexpected(reply_right.error().first);
    }

    auto msg = co_await dispatcher.get_message(reply_right.value());
    if (!msg) {
        kernelLogger() << "posixd: Error " << msg.error() << " waiting for read symlink reply from filesystem\n" << frg::endlog;
        co_return std::unexpected(msg.error());
    }

    if (msg->descriptor.size < sizeof(IPC_FS_Readlink_Reply)) {
        kernelLogger() << "posixd: Invalid read symlink reply size " << msg->descriptor.size << "\n" << frg::endlog;
        co_return std::unexpected(-EIO);
    }

    auto *reply = reinterpret_cast<IPC_FS_Readlink_Reply *>(msg->data.data());
    if (reply->type != IPC_FS_Readlink_Reply_NUM) {
        kernelLogger() << "posixd: Invalid read symlink reply type " << reply->type << "\n" << frg::endlog;
        co_return std::unexpected(-EIO);
    }

    if (reply->result_code != 0) {
        kernelLogger() << "posixd: Filesystem returned error " << reply->result_code << " reading symlink\n" << frg::endlog;
        co_return std::unexpected(reply->result_code);
    }

    co_return std::string(reply->path, msg->descriptor.size - sizeof(IPC_FS_Readlink_Reply));
}