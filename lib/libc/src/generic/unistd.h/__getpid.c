#include "__getpid.h"

#include <unistd.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <errno.h>

pmos_port_t __get_cmd_reply_port();
void __return_cmd_reply_port(pmos_port_t);

pmos_right_t __get_processd_right();

extern pid_t __pid_cached;

pid_t __getpid(int type)
{
    pmos_port_t reply_port = __get_cmd_reply_port();
    if (reply_port == INVALID_PORT) {
        return -1;
    }

    int extra_flags = 0;
    switch (type) {
        case GETPID_PPID:
            extra_flags = PID_FOR_TASK_PARENT_PID;
            break;
        case GETPID_PGID:
            extra_flags = PID_FOR_TASK_GROUP_ID;
            break;
    }

    IPC_PID_For_Task request = {
        .type = IPC_PID_For_Task_NUM,
        .task_id = TASK_ID_SELF,
        .flags = PID_FOR_TASK_WAIT_TO_APPEAR | extra_flags,
    };

    pmos_right_t processd_right = __get_processd_right();
    if (processd_right == INVALID_RIGHT) {
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    result_t result = send_message_right(processd_right, reply_port, &request, sizeof(request), NULL, 0).result;
    if (result != SUCCESS) {
        __return_cmd_reply_port(reply_port);
        errno = -result;
        return -1;
    }

    IPC_PID_For_Task_Reply reply;
    Message_Descriptor reply_descriptor;
    result = syscall_get_message_info(&reply_descriptor, reply_port, 0);
    if (result != SUCCESS) {
        // TODO: Catch EAGAIN
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    if (reply_descriptor.size != sizeof(IPC_PID_For_Task_Reply)) {
        __return_cmd_reply_port(reply_port);
        errno = EIO;
        return -1;
    }

    result = get_first_message((char *)&reply, MSG_ARG_REJECT_RIGHT, reply_port).result;
    if (result != SUCCESS) {
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    if (reply.type != IPC_PID_For_Task_Reply_NUM) {
        __return_cmd_reply_port(reply_port);
        errno = EIO;
        return -1;
    }

    if (reply.result != 0) {
        __return_cmd_reply_port(reply_port);
        errno = reply.result;
        return -1;
    }

    __return_cmd_reply_port(reply_port);

    return reply.pid;
}