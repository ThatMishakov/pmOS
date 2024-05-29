#include <unistd.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <pmos/tls.h>
#include <errno.h>
#include <stdio.h>

extern pmos_port_t worker_port;
pmos_port_t __get_cmd_reply_port();
void __libc_pre_fork();
void __libc_post_fork_child();
void __libc_post_fork_parent();

pid_t fork(void)
{
    pmos_port_t reply_port = __get_cmd_reply_port();
    if (reply_port == INVALID_PORT) {
        return -1;
    }

    IPC_Request_Fork request = {
        .type = IPC_Request_Fork_NUM,
        .flags = 0,
        .reply_port = reply_port,
    };

    __libc_pre_fork();

    result_t result = send_message_port(worker_port, sizeof(request), &request);
    if (result != 0) {
        errno = -result;
        goto error;
    }

    struct uthread *ut = __get_tls();
    while (1) {
        reply_port = ut->cmd_reply_port;

        IPC_Request_Fork_Reply reply;
        Message_Descriptor reply_descriptor;
        result = syscall_get_message_info(&reply_descriptor, reply_port, 0);
        if (result == -EINTR) {
            continue;
        } else if (result == -EPERM || result == -ENOENT) {
            if (reply_port == INVALID_PORT) {
                fprintf(stderr, "pmOS libC: fork() Interrupted\n");
                errno = EINTR;
                goto error;
            } else {
                continue;
            }
        } 

        if (reply_descriptor.size != sizeof(IPC_Request_Fork_Reply)) {
            fprintf(stderr, "pmOS libC: fork() Invalid message size %ld\n", reply_descriptor.size);
            errno = EAGAIN;
            goto error;
        }

        result = get_first_message((char *)&reply, sizeof(reply), reply_port);
        if (result != 0) {
            continue;
        }

        if (reply.result < 0) {
            errno = -reply.result;
            goto error;
        }

        if (reply.result == 0) {
            __libc_post_fork_child();
            return 0;
        } else {
            __libc_post_fork_parent();
            return reply.result;
        }
    }

error:
    __libc_post_fork_parent();
    return -1;
}
