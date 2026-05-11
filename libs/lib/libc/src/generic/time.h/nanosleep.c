#include <time.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <errno.h>
#include <pmos/tls.h>
#include <assert.h>
#include <pmos/__internal.h>
#include <stdbool.h>

pmos_port_t _HIDDEN __get_cmd_reply_port()
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

void _HIDDEN __return_cmd_reply_port(pmos_port_t)
{
    // Do nothing for now
}

static __thread pmos_right_t sleep_right = INVALID_RIGHT;

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    pmos_port_t reply_port = __get_cmd_reply_port();
    if (reply_port == INVALID_PORT) {
        return -1;
    }

    if (sleep_right == INVALID_RIGHT) {
        right_request_t right_request = pmos_create_timer(reply_port);
        if (right_request.result != SUCCESS) {
            errno = -right_request.result;
            goto error;
        }
        sleep_right = right_request.right;
    }

    size_t ns = req->tv_sec * 1000000000 + req->tv_nsec;

    result_t result = pmos_set_timer(reply_port, sleep_right, ns, PMOS_SET_TIMER_RELATIVE);
    if (result != SUCCESS) {
        errno = -result;
        goto error;
    }

    IPC_Timer_Expired reply;
    Message_Descriptor reply_descriptor;
    result = syscall_get_message_info(&reply_descriptor, reply_port, 0);

    // This should never fail
    assert(result == SUCCESS);

    // Check if the message size is correct
    assert(reply_descriptor.size == sizeof(reply));

    result = get_first_message((char *)&reply, MSG_ARG_REJECT_RIGHT, reply_port).result;

    __return_cmd_reply_port(reply_port);

    // This should never fail
    assert(result == SUCCESS);

    assert(reply.type == IPC_Timer_Expired_NUM);

    *rem = (struct timespec){0, 0};

    return 0;

error:
    if (rem) {
        *rem = *req;
    }
    __return_cmd_reply_port(reply_port);

    return -1;
}