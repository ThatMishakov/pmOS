#include <unistd.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <pmos/ports.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>

pmos_port_t __get_cmd_reply_port();
void __return_cmd_reply_port(pmos_port_t);

pmos_port_t __get_processd_port();

int kill(pid_t pid, int sig)
{
    if (sig < 0 || sig >= NSIG) {
        // The value of the sig argument is an invalid or unsupported signal number.
        errno = EINVAL;
        return -1;
    }

    pmos_port_t processd_port = __get_processd_port();
    if (processd_port == INVALID_PORT) {
        errno = ENOSYS;
        return -1;
    }

    pmos_port_t reply_port = __get_cmd_reply_port();
    if (reply_port == INVALID_PORT) {
        // errno is set
        return -1;
    }

    IPC_Kill request = {
        .type = IPC_Kill_NUM,
        .flags = 0,
        .reply_port = reply_port,
        .pid = pid,
        .signal = sig,
    };

    result_t result = send_message_port(processd_port, sizeof(request), &request);
    if (result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Error sending message to processd in kill(): %ld\n", result);
        errno = -result;
        goto error;
    }

    IPC_Kill_Reply reply;
    Message_Descriptor reply_descriptor;
    result = syscall_get_message_info(&reply_descriptor, reply_port, 0);
    if (result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Error getting message info in kill(): %ld\n", result);
        // TODO: Catch EAGAIN or whatever
        errno = -result;
        goto error;
    }

    if (reply_descriptor.size < sizeof(IPC_Kill_Reply)) {
        fprintf(stderr, "pmOS libC: Error in kill(): Unexpected message size: %lu\n", reply_descriptor.size);
        errno = EIO;
        goto error;
    }
    
    result = get_first_message((char *)&reply, MSG_ARG_REJECT_RIGHT, reply_port).result;
    if (result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Error getting message in kill(): %ld\n", result);
        errno = -result;
        goto error;
    }

    if (reply.type != IPC_Kill_Reply_NUM) {
        fprintf(stderr, "pmOS libC: Error in kill(): Unexpected message type: %d\n", reply.type);
        errno = EIO;
        goto error;
    }

    if (reply.result != 0) {
        errno = reply.result;
        goto error;
    }

    __return_cmd_reply_port(reply_port);
    return 0;
error:
    __return_cmd_reply_port(reply_port);
    return -1;
}