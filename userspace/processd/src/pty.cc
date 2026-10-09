#include "pty.hh"
#include <memory>
#include <pmos/async/coroutines.hh>
#include <pmos/ipc.h>
#include "log.hh"
#include <fcntl.h>
#include "vfs.hh"
#include "devfs.hh"
#include "process.hh"
#include <termios.h>
#include <pmos/fs-data.h>

struct PtyVfs final: public Filesystem {
    std::expected<std::shared_ptr<VNode>, int> resolve_child(std::shared_ptr<VNode> parent, const std::string &name) override
    {
        (void)parent;
        (void)name;

        return std::unexpected(-ENOENT);
    }

    pmos::async::task<std::expected<pmos::Right, int>> open_file(std::shared_ptr<VNode> vnode, std::shared_ptr<Process> process) override;

    pmos::async::task<std::expected<StatData, int>> get_file_stat_dynamic(std::shared_ptr<VNode> vnode)
    {
        if (vnode->type == FileType::Directory) {
            co_return StatData{
                .st_size = 0,
                .st_nlink = 2,
                .st_atim_tv_nsec = 0, .st_mtim_tv_nsec = 0,
                .st_ctim_tv_nsec = 0, .st_btim_tv_nsec = 0,
                .st_blocks = 0,
            };
        }

        co_return StatData{
            .st_size = 0,
            .st_nlink = 1,
            .st_atim_tv_nsec = 0, .st_mtim_tv_nsec = 0,
            .st_ctim_tv_nsec = 0, .st_btim_tv_nsec = 0,
            .st_blocks = 0,
        };
    }

    pmos::async::task<std::expected<void, int>> unlockpt(std::shared_ptr<VNode> vnode) override;
};

std::string PtyData::path()
{
    assert(vnode);
    return vnode->path();
}

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
    pty_root_vnode->st_mode = S_IFDIR | 0755;

    assert(devfs_root_vnode);
    devfs_root_vnode->children_cache[pty_root_vnode->name] = pty_root_vnode;
    pty_root_vnode->parent = devfs_root_vnode;
}

extern pmos::Port main_port;
extern pmos::PortDispatcher dispatcher;

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
    vnode->is_tty = true;
    pty->vnode = vnode;
    vnode->st_mode = S_IFCHR | 0620;

    pty_root_vnode->children_cache[vnode->name] = vnode;
    vnode->parent = pty_root_vnode;
    return vnode;
}

pmos::async::task<std::expected<void, int>> PtyVfs::unlockpt(std::shared_ptr<VNode> vnode)
{
    assert(vnode);
    assert(vnode->parent_fs.get() == this);

    auto pty_it = pty_map.find(vnode->inode);
    if (pty_it == pty_map.end()) {
        co_return std::unexpected(-EINVAL);
    }

    auto pty = pty_it->second.lock();
    if (!pty) {
        co_return std::unexpected(-EINVAL);
    }

    pty->locked = false;

    co_return {};
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

static void read_error_reply(pmos::Right reply_right, int error_code)
{
    IPC_Read_Reply reply = {
        .type        = IPC_Read_Reply_NUM,
        .flags       = 0,
        .result_code = static_cast<int16_t>(error_code),
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending read error reply to port " << reply_right.get() << "\n" << frg::endlog;
}

static void handle_read_reply(std::shared_ptr<PtyData> pty, size_t max_size, pmos::Right reply_right, bool subordinate)
{
    auto &queue = subordinate ? pty->subordinate_queue : pty->manager_queue;
    assert(!queue.empty());
    auto &packet = queue.front();
    size_t available_size = packet.data.size() - packet.offset;
    size_t to_read = std::min(max_size, available_size);

    std::vector<uint8_t> data(sizeof(IPC_Read_Reply) + to_read);
    IPC_Read_Reply *reply = reinterpret_cast<IPC_Read_Reply *>(data.data());
    reply->type = IPC_Read_Reply_NUM;
    reply->result_code = 0;
    reply->flags = 0;

    memcpy(data.data() + sizeof(IPC_Read_Reply), packet.data.data() + packet.offset, to_read);
    auto result_send = pmos::send_message_right(reply_right, std::span(data), {}, true);
    if (!result_send) {
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending read reply to port " << reply_right.get() << "\n" << frg::endlog;
    } else {
        packet.offset += to_read;
        if (packet.offset == packet.data.size()) {
            queue.pop_front();
        }
    }
}

static void handle_read(std::shared_ptr<PtyData> pty, IPC_Read *read_msg, pmos::Right reply_right)
{
    size_t read_size = read_msg->max_size;

    bool noblock = read_msg->flags & IPC_FLAG_IO_OP_NONBLOCK;

    if (pty->manager_queue.empty()) {
        // TODO: EIO on no subordinates

        if (noblock) {
            read_error_reply(std::move(reply_right), -EAGAIN);
            return;
        }
            
        pty->blocked_manager_reads.push_back({std::move(reply_right), read_size});
        return;
    }

    handle_read_reply(pty, read_size, std::move(reply_right), false);
}

static void handle_read_subordinate(std::shared_ptr<PtyData> pty, IPC_Read *read_msg, pmos::Right reply_right)
{
    size_t read_size = read_msg->max_size;

    bool noblock = read_msg->flags & IPC_FLAG_IO_OP_NONBLOCK;

    if (pty->subordinate_queue.empty()) {
        if (noblock) {
            read_error_reply(std::move(reply_right), -EAGAIN);
            return;
        }

        pty->blocked_subordinate_reads.push_back({std::move(reply_right), read_size});
        return;
    }

    handle_read_reply(pty, read_size, std::move(reply_right), true);
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
        case IPC_Read_NUM: {
            if (message.size() < sizeof(IPC_Read)) {
                kernelLogger() << "posixd: Received IPC_Read that is too small while attending pty\n" << frg::endlog;
                break;
            }

            IPC_Read *read_msg = reinterpret_cast<IPC_Read *>(message.data());

            handle_read(pty, read_msg, std::move(reply_right));
        }
            break;
        default:
            kernelLogger() << "posixd: Unknown message type " << ipc_msg->type << " while attending pty\n" << frg::endlog;
            break;
        }
    }

    co_return;
}

int ttiocsctty_handle(std::shared_ptr<PtyData> pty, unsigned flags, uint64_t sender_process_id)
{
    auto process = get_process_kernel_id(sender_process_id);
    if (!process) {
        kernelLogger() << "posixd: TIOCSCTTY request for pty " << pty->idx << " from unknown kernel process " << sender_process_id << "\n" << frg::endlog;
        return -EPERM;
    }

    auto session = process->process_group->session;
    if (process->pid != session->sid)
        return -EPERM;

    if (pty->session)
        return -EPERM;

    if (session->controlling_terminal)
        return -EPERM;

    pty->session = session;
    session->controlling_terminal = pty;

    return 0;
}

void process_out(const char c, PtyData::Packet &packet, std::shared_ptr<PtyData> pty)
{
    if (!(pty->active_settings.c_lflag & OPOST)) {
        packet.data.push_back(c);
        return;
    }

    if (pty->active_settings.c_oflag & ONLCR && c == '\n') {
        packet.data.push_back('\r');
        packet.data.push_back('\n');
        return;
    }

    packet.data.push_back(c);
}

void wake_up_manager(std::shared_ptr<PtyData> pty)
{
    while (!pty->blocked_manager_reads.empty() && !pty->manager_queue.empty()) {
        auto blocked_read = std::move(pty->blocked_manager_reads.front());
        pty->blocked_manager_reads.pop_front();
        
        handle_read_reply(pty, blocked_read.size, std::move(blocked_read.reply_right), false);
    }
}

void subordinate_handle_write(const char *data, size_t size, std::shared_ptr<PtyData> pty, pmos::Right reply_right)
{
    PtyData::Packet packet;
    for (size_t i = 0; i < size; i++) {
        process_out(data[i], packet, pty);
    }

    IPC_Write_Reply reply = {
        .type        = IPC_Write_Reply_NUM,
        .flags       = 0,
        .result_code = 0,
        .bytes_written        = static_cast<uint64_t>(packet.data.size()),
    };
    if (reply_right) {
        auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!result_send)
            kernelLogger() << "posixd: Error " << result_send.error().first << " sending write reply to port " << reply_right.get() << "\n" << frg::endlog;
    }

    pty->manager_queue.push_back(std::move(packet));
    wake_up_manager(pty);
}

pmos::async::detached_task openpt_subordinate(pmos::ReceiveRight rr, std::shared_ptr<PtyData> pty, unsigned oflags, std::shared_ptr<Process> process)
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
        case IPC_Ioctl_NUM: {
            if (message.size() < sizeof(IPC_Ioctl)) {
                kernelLogger() << "posixd: Received IPC_Ioctl that is too small while attending pty subordinate\n" << frg::endlog;
                break;
            }

            auto *ioctl_msg = reinterpret_cast<IPC_Ioctl *>(ipc_msg);
            int result_code = 0;

            switch (ioctl_msg->request) {
            case TIOCSCTTY:
                result_code = ttiocsctty_handle(pty, ioctl_msg->flags, msg.sender_process);
                break;
            default:
                result_code = -ENOTTY;
                break;
            }

            IPC_Ioctl_Reply reply = {
                .type         = IPC_Ioctl_Reply_NUM,
                .flags        = 0,
                .result_code  = static_cast<int16_t>(result_code),
                .ioctl_result = 0,
            };

            auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
            if (!result_send)
                kernelLogger() << "posixd: Error " << result_send.error().first << " sending ioctl reply to port " << reply_right.get() << "\n" << frg::endlog;
        }
            break;
        case IPC_Write_NUM: {
            if (message.size() < sizeof(IPC_Write)) {
                kernelLogger() << "posixd: Received IPC_Write that is too small while attending pty subordinate\n" << frg::endlog;
                break;
            }

            auto *write_msg = reinterpret_cast<IPC_Write *>(ipc_msg);
            auto size = message.size() - sizeof(IPC_Write);
            subordinate_handle_write(write_msg->data, size, pty, std::move(reply_right));
        }
            break;
        case IPC_Read_NUM: {
            if (message.size() < sizeof(IPC_Read)) {
                kernelLogger() << "posixd: Received IPC_Read that is too small while attending pty subordinate\n" << frg::endlog;
                break;
            }

            IPC_Read *read_msg = reinterpret_cast<IPC_Read *>(message.data());

            handle_read_subordinate(pty, read_msg, std::move(reply_right));
        }
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

    auto op_right_result = main_port.create_right(pmos::RightType::SendMany);
    attend_open_file(pty->vnode, std::move(op_right_result.value().second));

    auto io_right = std::move(right.value().first);
    auto op_right = std::move(op_right_result.value().first);

    uint16_t flags = O_RDWR | (oflags & O_NONBLOCK) | FLAG_ISATTY;

    IPC_Open_Reply reply = {
        .type        = IPC_Open_Reply_NUM,
        .result_code = 0,
        .fs_flags    = flags,
    };

    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true, std::move(op_right), std::move(io_right));
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending openpt reply to port " << reply_right.get() << "\n" << frg::endlog;
}

pmos::async::task<std::expected<pmos::Right, int>> PtyVfs::open_file(std::shared_ptr<VNode> vnode, std::shared_ptr<Process> process)
{
    auto pty = pty_map[vnode->inode].lock();
    assert(pty);
    assert(process);

    if (pty->locked)
        co_return std::unexpected(-EACCES);

    auto [right, receive_right] = main_port.create_right(pmos::RightType::SendMany).value();

    openpt_subordinate(std::move(receive_right), pty, 0, process);
    pty->subordinate_count++;

    co_return std::move(right);
}