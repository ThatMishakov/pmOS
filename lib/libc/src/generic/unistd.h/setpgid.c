#include <unistd.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <errno.h>

pmos_port_t __get_cmd_reply_port();
void __return_cmd_reply_port(pmos_port_t);

pmos_port_t __get_processd_port();

int setpgid(pid_t pid, pid_t pgid)
{
    pmos_port_t reply_port = __get_cmd_reply_port();
    if (reply_port == INVALID_PORT) {
        return -1;
    }

    IPC_Set_Process_Group request = {
        .type = IPC_Set_Process_Group_NUM,
        .flags = 0,
        .reply_port = reply_port,
        .pid = pid,
        .pgid = pgid,
    };

    pmos_port_t processd_port = __get_processd_port();
    if (processd_port == INVALID_PORT) {
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    result_t result = send_message_port(processd_port, sizeof(request), &request);
    if (result != SUCCESS) {
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    IPC_Set_Process_Group_Reply reply;
    Message_Descriptor reply_descriptor;

    result = syscall_get_message_info(&reply_descriptor, reply_port, 0);
    if (result != SUCCESS) {
        // TODO: Catch EAGAIN
        errno = EIO;
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    if (reply_descriptor.size != sizeof(IPC_Set_Process_Group_Reply)) {
        errno = EIO;
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    result = get_first_message((char *)&reply, MSG_ARG_REJECT_RIGHT, reply_port).result;
    if (result != SUCCESS) {
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    if (reply.type != IPC_Set_Process_Group_Reply_NUM) {
        errno = EIO;
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    if (reply.result != 0) {
        errno = reply.result;
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    __return_cmd_reply_port(reply_port);

    return 0;
}