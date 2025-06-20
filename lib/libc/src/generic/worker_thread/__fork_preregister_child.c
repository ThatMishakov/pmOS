#include <pmos/system.h>
#include <pmos/tls.h>
#include <pmos/ipc.h>
#include <stdio.h>
#include <errno.h>
#include <pmos/__internal.h>

// TODO: Move this to a header
pmos_port_t __get_cmd_reply_port();
void __return_cmd_reply_port(pmos_port_t);
pmos_port_t __get_processd_right();

_HIDDEN pid_t __fork_preregister_child(uint64_t tid)
{
    pmos_port_t reply_port = __get_cmd_reply_port();
    if (reply_port == INVALID_PORT) {
        return -1;
    }

    IPC_Preregister_Process request = {
        .type = IPC_Preregister_Process_NUM,
        .flags = 0,
        .worker_task_id = tid,
        .parent_pid = 0,
    };

    pmos_port_t processd_right = __get_processd_right();
    if (processd_right == INVALID_RIGHT) {
        fprintf(stderr, "pmOS libC: Failed to get processd right\n");
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    result_t result = send_message_right(processd_right, reply_port, &request, sizeof(request), NULL, 0).result;
    if (result != SUCCESS) {
        __return_cmd_reply_port(reply_port);
        errno = -result;
        return -1;
    }

    IPC_Preregister_Process_Reply reply;
    Message_Descriptor reply_descriptor;
    result = syscall_get_message_info(&reply_descriptor, reply_port, 0);
    if (result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Failed to get preregister process reply info: %ld\n", result);
        errno = -result;
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    if (reply_descriptor.size != sizeof(IPC_Preregister_Process_Reply)) {
        fprintf(stderr, "pmOS libC: Unexpected reply size for preregister process: %ld\n", reply_descriptor.size);
        get_first_message((char *)&reply, sizeof(reply), reply_port);
        errno = EAGAIN;
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    result = get_first_message((char *)&reply, MSG_ARG_REJECT_RIGHT, reply_port).result;
    if (result != SUCCESS) {
        printf("Reply port: %ld\n", reply_port);
        fprintf(stderr, "pmOS libC: Failed to get preregister process reply: %li\n", result);
        errno = -result;
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    if (reply.type != IPC_Preregister_Process_Reply_NUM) {
        fprintf(stderr, "pmOS libC: Unexpected reply type for preregister process: %d\n", reply.type);
        errno = EAGAIN;
        __return_cmd_reply_port(reply_port);
        return -1;
    }

    if (reply.result < 0) {
        fprintf(stderr, "pmOS libC: Failed to preregister process: %ld\n", reply.result);
        __return_cmd_reply_port(reply_port);
        errno = -reply.result;
        return -1;
    }

    __return_cmd_reply_port(reply_port);

    return reply.result;
}