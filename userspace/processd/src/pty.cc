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
#include <signal.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <span>

struct PtyVfs final: public Filesystem {
    std::expected<std::shared_ptr<VNode>, int> resolve_child(std::shared_ptr<VNode> parent, const std::string &name) override
    {
        (void)parent;
        (void)name;

        return std::unexpected(-ENOENT);
    }

    pmos::async::task<std::expected<pmos::Right, int>> open_file(std::shared_ptr<VNode> vnode, std::shared_ptr<Process> process) override;

    pmos::async::task<std::expected<StatData, int>> get_file_stat_dynamic(std::shared_ptr<VNode> vnode) override
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

static void termios_default(struct termios *t)
{
    memset(t, 0, sizeof(*t));
    t->c_iflag = ICRNL | IXON;
    t->c_oflag = OPOST | ONLCR;
    t->c_cflag = CS8 | CREAD;
    t->c_lflag = TTYDEF_LFLAG | ECHOK;
	t->c_cc[VINTR] = CINTR;
	t->c_cc[VEOF] = CEOF;
	t->c_cc[VKILL] = CKILL;
	t->c_cc[VSTART] = CSTART;
	t->c_cc[VSTOP] = CSTOP;
	t->c_cc[VSUSP] = CSUSP;
	t->c_cc[VQUIT] = CQUIT;
	t->c_cc[VERASE] = CERASE; // DEL character.
	t->c_cc[VMIN] = CMIN;
	t->c_cc[VDISCARD] = CDISCARD;
	t->c_cc[VLNEXT] = CLNEXT;
	t->c_cc[VWERASE] = CWERASE;
	t->c_cc[VREPRINT] = CRPRNT;
	cfsetispeed(t, B38400);
	cfsetospeed(t, B38400);
}

std::expected<std::shared_ptr<PtyData>, int> new_pty()
{
    static unsigned next_idx = 0;
    auto pty = std::make_shared<PtyData>();
    termios_default(&pty->active_settings);
    pty->idx = next_idx++;
    pty_map[pty->idx] = pty;
    pty->vnode = create_pty_vnode(pty);
    return pty;
}

static unsigned poll_events(const std::shared_ptr<PtyData> &pty, bool subordinate)
{
    unsigned events = 0;

    auto &queue = subordinate ? pty->subordinate_queue : pty->manager_queue;
    if (!queue.empty())
        events |= POLLIN | POLLRDNORM;

    events |= POLLOUT | POLLWRNORM;

    return events;
}

static void wakeup_polls(const std::shared_ptr<PtyData> &pty, bool subordinate)
{
    uint16_t events = poll_events(pty, subordinate);
    auto &queue = subordinate ? pty->subordinate_polls : pty->manager_polls;

    auto it = queue.begin();
    while (it != queue.end()) {
        auto &pending_poll = *it;
        uint16_t poll_events = events & pending_poll.mask;

        if (poll_events != 0) {
            IPC_Poll_Reply reply = {
                .type        = IPC_Poll_Reply_NUM,
                .flags       = 0,
                .result_code = 0,
                .events      = poll_events,
            };

            auto send_result = pmos::send_message_right_one(pending_poll.reply_right, reply, {}, true);
            if (!send_result) {
                kernelLogger() << "posix: Error " << send_result.error().first << " sending message to port " << pending_poll.reply_right.get() << " for pipe_poll\n" << frg::endlog;
            }

            it = queue.erase(it);
        } else {
            ++it;
        }
    }
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

void process_out(const char c, PtyData::Packet &packet, std::shared_ptr<PtyData> pty)
{
    if (!(pty->active_settings.c_oflag & OPOST)) {
        packet.data.push_back(c);
        return;
    }

    if ((pty->active_settings.c_oflag & ONLCR) && c == '\n') {
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

void wake_up_subordinate(std::shared_ptr<PtyData> pty)
{
    while (!pty->blocked_subordinate_reads.empty() && !pty->subordinate_queue.empty()) {
        auto blocked_read = std::move(pty->blocked_subordinate_reads.front());
        pty->blocked_subordinate_reads.pop_front();

        handle_read_reply(pty, blocked_read.size, std::move(blocked_read.reply_right), true);
    }

    wakeup_polls(pty, true);
}

void enqueue_packet(PtyData::Packet &packet, std::shared_ptr<PtyData> pty)
{
    if (packet.data.empty())
        return;

    pty->subordinate_queue.push_back(std::move(packet));
    wake_up_subordinate(pty);
}

// This was "inspired by" Managarm's implementation
// managarm/posix/subsystem/src/pts.cpp
void process_in(char c, PtyData::Packet &packet, std::shared_ptr<PtyData> pty)
{
    auto enqueue_out = [&](PtyData::Packet &packet) {
        if (packet.data.empty())
            return;
        pty->manager_queue.push_back(std::move(packet));
        wake_up_manager(pty);
    };

    auto is_control_char = [](char c) {
        return (c >= 0 && c < 0x20) || (c == 0x7F);
    };

    auto erase_char = [&](bool erase) {
        if (!packet.data.empty()) {
            size_t chars = 1;
            char c = packet.data.back();
            packet.data.pop_back();

            if (is_control_char(c))
                chars = 2;

            if ((pty->active_settings.c_lflag & ECHO) && erase) {
                PtyData::Packet echo_packet;
                for (size_t i = 0; i < chars; i++) {
                    echo_packet.data.push_back('\b');
                    echo_packet.data.push_back(' ');
                    echo_packet.data.push_back('\b');
                }
                enqueue_out(echo_packet);
            }
        }
    };

    if (pty->active_settings.c_iflag & ISTRIP)
        c &= 0x7F;

    if (c == '\r') {
        if (pty->active_settings.c_iflag & IGNCR)
            return;

        if (pty->active_settings.c_iflag & ICRNL)
            c = '\n';
    } else if (c == '\n') {
        if (pty->active_settings.c_iflag & INLCR)
            c = '\r';
    }

    if ((pty->active_settings.c_iflag & IUCLC) && (c >= 'A') && (c <= 'Z'))
        c = c - 'A' + 'a';

    if (pty->active_settings.c_lflag & ISIG) {
        std::optional<int> signal = std::nullopt;

        if (c == static_cast<char>(pty->active_settings.c_cc[VINTR]))
            signal = SIGINT;
        else if (c == static_cast<char>(pty->active_settings.c_cc[VQUIT]))
            signal = SIGQUIT;
        else if (c == static_cast<char>(pty->active_settings.c_cc[VSUSP]))
            signal = SIGTSTP;

        if (signal)
            pty->issue_signal_to_fg(*signal);
    }

    if (pty->active_settings.c_lflag & ICANON) {
        if (c == static_cast<char>(pty->active_settings.c_cc[VKILL])) {
            while (!packet.data.empty())
                erase_char(pty->active_settings.c_lflag & ECHOK);

            return;
        }

        if (c == static_cast<char>(pty->active_settings.c_cc[VERASE])) {
            erase_char(pty->active_settings.c_lflag & ECHOE);
            return;
        }

        if ((pty->active_settings.c_lflag & IEXTEN) && (c == static_cast<char>(pty->active_settings.c_cc[VWERASE]))) {
            while (!packet.data.empty() && packet.data.back() == ' ')
                erase_char(pty->active_settings.c_lflag & ECHOE);

            while (!packet.data.empty() && packet.data.back() != ' ')
                erase_char(pty->active_settings.c_lflag & ECHOE);

            return;
        }

        if (c == static_cast<char>(pty->active_settings.c_cc[VEOF])) {
            enqueue_packet(packet, pty);
            packet = {};

            return;
        }
    }

    char echo_char = (pty->active_settings.c_lflag & ECHO) ? c : '\0';

    if ((pty->active_settings.c_lflag & ECHOCTL) && (pty->active_settings.c_lflag & ECHO) && (c < 0x20 && c != '\n' && c != '\t')) {
        PtyData::Packet echo_packet;
        echo_packet.data.push_back('^');
        echo_packet.data.push_back(c + 0x40);
        enqueue_out(echo_packet);
        echo_char = '\0';
    }

    if (pty->active_settings.c_lflag & ICANON) {
        packet.data.push_back(c);

        if (echo_char) {
            PtyData::Packet echo_packet;
            if (is_control_char(c) && c != '\n') {
                echo_packet.data.push_back('^');
                echo_packet.data.push_back(('@' + c) % 128);
            } else {
                echo_packet.data.push_back(echo_char);
            }
            enqueue_out(echo_packet);
        }

        if (c == '\n' || (c == static_cast<char>(pty->active_settings.c_cc[VEOL])
            || (c == static_cast<char>(pty->active_settings.c_cc[VEOL2])))) {
            if (!(pty->active_settings.c_lflag & ECHO) && (pty->active_settings.c_lflag & ECHONL)) {
                PtyData::Packet echo_packet;
                echo_packet.data.push_back('\n');
                enqueue_out(echo_packet);
            }
            enqueue_packet(packet, pty);
            packet = {};
            return;
        }

        return;
    } else if (pty->active_settings.c_lflag & ECHO) {
        PtyData::Packet echo_packet;
        echo_packet.data.push_back(c);
        enqueue_out(echo_packet);
    }

    packet.data.push_back(c);
    return;
}

void manager_handle_write(const char *data, size_t size, std::shared_ptr<PtyData> pty, pmos::Right reply_right)
{
    for (size_t i = 0; i < size; i++)
        process_in(data[i], pty->active_packet, pty);

    if (!(pty->active_settings.c_lflag & ICANON)) {
        enqueue_packet(pty->active_packet, pty);
        pty->active_packet = {};
    }

    IPC_Write_Reply reply = {
        .type          = IPC_Write_Reply_NUM,
        .flags         = 0,
        .result_code   = 0,
        .bytes_written = static_cast<uint64_t>(size),
    };
    if (reply_right) {
        auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!result_send)
            kernelLogger() << "posixd: Error " << result_send.error().first << " sending write reply to port " << reply_right.get() << "\n" << frg::endlog;
    }
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
        case IPC_Write_NUM: {
            if (message.size() < sizeof(IPC_Write)) {
                kernelLogger() << "posixd: Received IPC_Write that is too small while attending pty subordinate\n" << frg::endlog;
                break;
            }

            auto *write_msg = reinterpret_cast<IPC_Write *>(ipc_msg);
            auto size = message.size() - sizeof(IPC_Write);
            manager_handle_write(write_msg->data, size, pty, std::move(reply_right));
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

void subordinate_handle_write(const char *data, size_t size, std::shared_ptr<PtyData> pty, pmos::Right reply_right)
{
    PtyData::Packet packet;
    for (size_t i = 0; i < size; i++) {
        process_out(data[i], packet, pty);
    }

    IPC_Write_Reply reply = {
        .type          = IPC_Write_Reply_NUM,
        .flags         = 0,
        .result_code   = 0,
        .bytes_written = size,
    };
    if (reply_right) {
        auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!result_send)
            kernelLogger() << "posixd: Error " << result_send.error().first << " sending write reply to port " << reply_right.get() << "\n" << frg::endlog;
    }

    pty->manager_queue.push_back(std::move(packet));
    wake_up_manager(pty);
}

static void handle_poll(std::shared_ptr<PtyData> pty, uint16_t flags, uint16_t mask, pmos::Right reply_right, bool subordinate)
{
    uint16_t events = poll_events(pty, subordinate);
    uint16_t poll_events = events & mask;

    if (poll_events != 0 || (flags & IPC_POLL_FLAG_NONBLOCK)) {
        IPC_Poll_Reply reply = {
            .type        = IPC_Poll_Reply_NUM,
            .flags       = 0,
            .result_code = 0,
            .events      = poll_events,
        };

        auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!result_send)
            kernelLogger() << "posixd: Error " << result_send.error().first << " sending poll reply to port " << reply_right.get() << "\n" << frg::endlog;
    } else {
        auto &queue = subordinate ? pty->subordinate_polls : pty->manager_polls;
        queue.push_back({std::move(reply_right), mask});
    }
}

void handle_ioctl(std::shared_ptr<PtyData> pty, unsigned request, unsigned flags, std::span<std::byte> data, pmos::Right reply_right, uint64_t sender_process_id)
{
    int result_code = 0;

    switch (request) {
    case TCSETS:
    // The next two are a TODO!!
    case TCSETSW:
    case TCSETSF: {
        if (data.size() < sizeof(struct termios)) {
            result_code = -EINVAL;
            break;
        }
        struct termios *new_settings = reinterpret_cast<struct termios *>(data.data());
        pty->active_settings = *new_settings;
    
        result_code = 0;
    }
        break;
    case TIOCSCTTY:
        result_code = ttiocsctty_handle(pty, flags, sender_process_id);
        break;
    default:
        kernelLogger() << "posixd: Unknown ioctl request " << request << " while attending pty\n" << frg::endlog;
        result_code = -ENOTTY;
        break;
    }

    IPC_Ioctl_Reply reply = {
        .type         = IPC_Ioctl_Reply_NUM,
        .flags        = 0,
        .result_code  = static_cast<int16_t>(result_code),
        .data = {},
    };
    auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
    if (!result_send)
        kernelLogger() << "posixd: Error " << result_send.error().first << " sending ioctl reply to port " << reply_right.get() << "\n" << frg::endlog;
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
        case IPC_Tcgetattr_NUM: {
            IPC_Tcgetattr_Reply reply = {
                .type = IPC_Tcgetattr_Reply_NUM,
                .result_code = 0,
                .termios = pty->active_settings,
            };

            auto result_send = pmos::send_message_right_one(reply_right, reply, {}, true);
            if (!result_send)
                kernelLogger() << "posixd: Error " << result_send.error().first << " sending termios getattr reply to port " << reply_right.get() << "\n" << frg::endlog;
        }
            break;
        case IPC_Poll_NUM: {
            if (message.size() < sizeof(IPC_Poll)) {
                kernelLogger() << "posixd: Received IPC_Poll that is too small while attending pty subordinate\n" << frg::endlog;
                break;
            }

            IPC_Poll *poll_msg = reinterpret_cast<IPC_Poll *>(message.data());
            handle_poll(pty, poll_msg->flags, poll_msg->events, std::move(reply_right), true);
        }
            break;
        case IPC_Ioctl_NUM: {
            if (message.size() < sizeof(IPC_Ioctl)) {
                kernelLogger() << "posixd: Received IPC_Ioctl that is too small while attending pty subordinate\n" << frg::endlog;
                break;
            }
            IPC_Ioctl *ioctl_msg = reinterpret_cast<IPC_Ioctl *>(message.data());

            auto span = std::span(message.data(), message.size());
            span = span.subspan(sizeof(IPC_Ioctl));
            handle_ioctl(pty, ioctl_msg->request, ioctl_msg->flags, span, std::move(reply_right), msg.sender_process);
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

void PtyData::issue_signal_to_fg(int signal)
{
    kernelLogger() << "posixd: stub issue_signal_to_fg for signal " << signal << " on pty " << idx << "\n" << frg::endlog;   
}