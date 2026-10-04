#include "pty.hh"
#include <memory>
#include <pmos/async/coroutines.hh>
#include <pmos/ipc.h>
#include "log.hh"
#include <fcntl.h>
#include "vfs.hh"
#include "devfs.hh"

struct PtyVfs final: public Filesystem {
    std::expected<std::shared_ptr<VNode>, int> resolve_child(std::shared_ptr<VNode> parent, const std::string &name) override
    {
        (void)parent;
        (void)name;

        return std::unexpected(-ENOENT);
    }

    pmos::async::task<std::expected<pmos::Right, int>> open_file(std::shared_ptr<VNode> vnode) override;

    pmos::async::task<std::expected<StatData, int>> get_file_stat_dynamic(std::shared_ptr<VNode> vnode)
    {
        kernelLogger() << "posixd: Attempted to get file stat on the pty filesystem\n" << frg::endlog;
        co_return std::unexpected(-ENOSYS); // TODO
    }
};

std::shared_ptr<PtyVfs> pty_fs;
std::shared_ptr<VNode> pty_root_vnode;

void init_pty_filesystem()
{
    pty_fs = std::make_shared<PtyVfs>();

    pty_root_vnode = std::make_shared<VNode>();
    pty_root_vnode->type = FileType::Directory;
    pty_root_vnode->parent_fs = pty_fs;
    pty_fs->root = pty_root_vnode;
    pty_root_vnode->name = "pts";

    assert(devfs_root_vnode);
    devfs_root_vnode->children_cache[pty_root_vnode->name] = pty_root_vnode;
    pty_root_vnode->parent = devfs_root_vnode;
}

extern pmos::Port main_port;
extern pmos::PortDispatcher dispatcher;

struct PtyData {
    bool have_manager = true;
    bool locked = true;

    size_t subordinate_count = 0;

    unsigned idx = 0;

    std::string name() const {
        return std::to_string(idx);
    }

    std::string path() {
        assert(vnode);
        return vnode->path();
    }

    std::shared_ptr<VNode> vnode = nullptr;

    ~PtyData();
};

std::map<unsigned, std::weak_ptr<PtyData>> pty_map;

PtyData::~PtyData()
 {
    if (vnode) {
        auto parent = vnode->parent.lock();
        if (parent) {
            parent->children_cache.erase(vnode->name);
        }
    }

    pty_map.erase(idx);
}

std::shared_ptr<VNode> create_pty_vnode(std::shared_ptr<PtyData> pty)
{
    auto vnode = std::make_shared<VNode>();
    vnode->parent_fs = pty_fs;
    vnode->type = FileType::CharacterDevice;
    vnode->st_mode = 0666;
    vnode->st_uid = 0;
    vnode->st_gid = 0;
    vnode->name = pty->name();
    vnode->inode = pty->idx;
    pty->vnode = vnode;

    pty_root_vnode->children_cache[vnode->name] = vnode;
    vnode->parent = pty_root_vnode;
    return vnode;
}

std::expected<std::shared_ptr<PtyData>, int> new_pty()
{
    static unsigned next_idx = 0;
    auto pty = std::make_shared<PtyData>();
    pty->idx = next_idx++;
    pty_map[pty->idx] = pty;
    pty->vnode = create_pty_vnode(pty);
    return pty;
}

pmos::async::detached_task openpt_manager(pmos::ReceiveRight rr, std::shared_ptr<PtyData> pty, unsigned oflags)
{
    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message(rr)).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posixd: Received very small message while attending pty\n" << frg::endlog;
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            break;
        case IPC_Unlockpt_NUM: {
            if (message.size() < sizeof(IPC_Unlockpt)) {
                kernelLogger() << "posixd: Received IPC_Unlockpt that is too small while attending pty\n" << frg::endlog;
                break;
            }

            pty->locked = false;

            IPC_Unlockpt_Reply reply = {
                .type        = IPC_Unlockpt_Reply_NUM,
                .result_code = 0,
                .flags       = 0,
            };

            auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
            if (!result_send)
                kernelLogger() << "posixd: Error " << result_send.error().first << " sending unlockpt reply to port " << reply_right.get() << "\n" << frg::endlog;
        } break;
        case IPC_Ttyname_NUM: {
            if (message.size() < sizeof(IPC_Ttyname)) {
                kernelLogger() << "posixd: Received IPC_Ttyname that is too small while attending pty\n" << frg::endlog;
                break;
            }

            auto path = pty->path();
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
            break;
        default:
            kernelLogger() << "posixd: Unknown message type " << ipc_msg->type << " while attending pty\n" << frg::endlog;
            break;
        }
    }

    co_return;
}

pmos::async::detached_task openpt_subordinate(pmos::ReceiveRight rr, std::shared_ptr<PtyData> pty, unsigned oflags)
{
    while (1) {
        auto [msg, message, reply_right, rights] = (co_await dispatcher.get_message(rr)).value();

        if (message.size() < sizeof(IPC_Generic_Msg)) {
            kernelLogger() << "posixd: Received very small message while attending pty subordinate\n" << frg::endlog;
            break;
        }

        auto *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(message.data());
        switch (ipc_msg->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            break;
        default:
            kernelLogger() << "posixd: Unknown message type " << ipc_msg->type << " while attending pty subordinate\n" << frg::endlog;
            break;
        }
    }

    co_return;
}

void openpt_error(pmos::Right &reply_right, int16_t result)
{
    if (!reply_right)
        return;

    IPC_Open_Reply reply = {
        .type        = IPC_Open_Reply_NUM,
        .result_code = result,
        .fs_flags    = 0,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending openpt reply to port " << reply_right.get() << "\n" << frg::endlog;
}

void openpt_handle(pmos::Right reply_right, unsigned oflags)
{
    if (!reply_right)
        return;

    auto pty_e = new_pty();
    if (!pty_e) {
        openpt_error(reply_right, pty_e.error());
        return;
    }
    auto pty = std::move(pty_e.value());

    auto right = main_port.create_right(pmos::RightType::SendMany);
    openpt_manager(std::move(right.value().second), pty, oflags);

    auto op_right = std::move(right.value().first);
    auto io_right = op_right.clone();

    uint16_t flags = O_RDWR | (oflags & O_NONBLOCK);

    IPC_Open_Reply reply = {
        .type        = IPC_Open_Reply_NUM,
        .result_code = 0,
        .fs_flags    = flags,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true, std::move(op_right), std::move(io_right));
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending openpt reply to port " << reply_right.get() << "\n" << frg::endlog;
}

pmos::async::task<std::expected<pmos::Right, int>> PtyVfs::open_file(std::shared_ptr<VNode> vnode)
{
    auto pty = pty_map[vnode->inode].lock();
    assert(pty);

    if (pty->locked)
        co_return std::unexpected(-EACCES);

    auto [right, receive_right] = main_port.create_right(pmos::RightType::SendMany).value();

    openpt_subordinate(std::move(receive_right), pty, 0);
    pty->subordinate_count++;

    co_return std::move(right);
}