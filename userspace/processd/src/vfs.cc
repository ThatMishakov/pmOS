#include <pmos/async/coroutines.hh>
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
        printf("vfsd: Error %d sending mount filesystem reply to port %" PRIu64 "\n", result_send.error(),
               reply_right.get());
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
    vnode->is_dir = true;

    fs->root = vnode;
    filesystems.push_back(fs);
    root_vnode = vnode;

    printf("posix: Mounted filesystem at %s with root inode %" PRId64 "\n", fs->mountpoint.c_str(), root_inode);

    mount_filesystem_reply(reply_right, 0);
}

pmos::async::task<std::expected<std::shared_ptr<VNode>, int>> resolve_path(std::string path);

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
        printf("vfsd: Error %d sending open file message to filesystem\n", reply_right.error());
        co_return std::unexpected(reply_right.error());
    }

    auto msg = co_await dispatcher.get_message(reply_right.value());
    if (!msg) {
        printf("vfsd: Error %d waiting for open file reply from filesystem\n", msg.error());
        co_return std::unexpected(msg.error());
    }

    if (msg->descriptor.size < sizeof(IPC_FS_Open_Reply)) {
        printf("vfsd: Invalid open file reply size %li\n", msg->descriptor.size);
        co_return std::unexpected(-EIO);
    }

    auto *reply = reinterpret_cast<IPC_FS_Open_Reply *>(msg->data.data());
    if (reply->type != IPC_FS_Open_Reply_NUM) {
        printf("vfsd: Invalid open file reply type %d\n", reply->type);
        co_return std::unexpected(-EIO);
    }

    if (reply->result_code != 0) {
        printf("vfsd: Filesystem returned error %d opening file\n", reply->result_code);
        co_return std::unexpected(-reply->result_code);
    }

    if (!msg->other_rights[0] || msg->other_rights[0].type() != pmos::RightType::SendMany) {
        printf("vfsd: Invalid open file reply: missing file right\n");
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
        printf("vfsd: Error %d sending open file reply to port %" PRIu64 "\n", result_send.error(),
               reply_right.get());
}

pmos::async::detached_task attend_open_file(std::shared_ptr<VNode> vnode, pmos::RecieveRight right)
{
    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message(right)).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            printf("Warning: recieved very small message\n");
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        default:
            printf("vfsd: Unknown message type %d while attending file\n", ipc_msg->type);
            break;
        }
    }
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
        printf("vfsd: Error %d sending open file reply to port %" PRIu64 "\n", send_result.error(), reply_right.get());
}

pmos::async::detached_task vfs_handle_messages()
{
    auto right = main_port.create_right(pmos::RightType::SendMany);
    auto [r, recieve_right] = std::move(right.value());
    auto result = pmos::name_right(std::move(r), "/pmos/vfsd");

    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message_default()).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            printf("Warning: recieved very small message\n");
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Mount_FS_NUM: {
            if (message.size() < sizeof(IPC_Mount_FS)) {
                printf("vfsd: Recieved IPC_Mount_FS that is too small from task %li of size %li\n",
                       msg.sender, message.size());
                break;
            }

            auto *m = reinterpret_cast<IPC_Mount_FS *>(message.data());
            std::string mountpoint(m->mount_path, message.size() - sizeof(IPC_Mount_FS));
            mount_filesystem(std::move(reply_right), std::move(rights[0]), mountpoint, m->root_fd);
        } break;
        case IPC_Open_NUM: {
            if (message.size() < sizeof(IPC_Open)) {
                printf("vfsd: Recieved IPC_Open that is too small from task %li of size %li\n", msg.sender,
                       message.size());
                break;
            }

            auto *m = reinterpret_cast<IPC_Open *>(message.data());
            std::string path(m->path, message.size() - sizeof(IPC_Open));
            open_file(std::move(reply_right), path);
        } break;
        default:
            printf("vfsd: Unknown message type %d\n", ipc_msg->type);
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