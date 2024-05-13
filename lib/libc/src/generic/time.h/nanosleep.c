#include <time.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <errno.h>
#include <pmos/tls.h>
#include <assert.h>

int pmos_request_timer(pmos_port_t port, size_t ms);

pmos_port_t get_sleep_port()
{
    struct uthread *ut = __get_tls();
    if (ut->cmd_reply_port == INVALID_PORT) {
        ports_request_t port_request = create_port(TASK_ID_SELF, 0);
        if (port_request.result != SUCCESS) {
            errno = port_request.result;
            return INVALID_PORT;
        }
        ut->cmd_reply_port = port_request.port;
    }
    return ut->cmd_reply_port;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    pmos_port_t sleep_reply_port = get_sleep_port();
    size_t ns                    = req->tv_sec * 1000000000 + req->tv_nsec;
    result_t result              = pmos_request_timer(sleep_reply_port, ns);
    if (result != SUCCESS) {
        errno = result;
        *rem  = *req;
        return -1;
    }

    IPC_Timer_Reply reply;
    Message_Descriptor reply_descriptor;
    result = syscall_get_message_info(&reply_descriptor, sleep_reply_port, 0);

    // This should never fail
    assert(result == SUCCESS);

    // Check if the message size is correct
    assert(reply_descriptor.size == sizeof(IPC_Timer_Reply));

    result = get_first_message((char *)&reply, sizeof(reply), sleep_reply_port);

    // This should never fail
    assert(result == SUCCESS);

    assert(reply.type == IPC_Timer_Reply_NUM);

    if (reply.status < 0) {
        errno = -reply.status;
        *rem  = *req;
        return -1;
    }

    *rem = (struct timespec){0, 0};

    return 0;
}