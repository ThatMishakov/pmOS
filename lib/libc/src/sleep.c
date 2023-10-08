#include <unistd.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

static __thread pmos_port_t sleep_reply_port = INVALID_PORT;

// TODO
static pmos_port_t get_sleep_port() {
    static pmos_port_t sleep_port = INVALID_PORT;
    static const char sleep_port_name[] = "/pmos/devicesd";
    if (sleep_port == INVALID_PORT) {
        ports_request_t port_req = get_port_by_name(sleep_port_name, strlen(sleep_port_name), 0);
        if (port_req.result != SUCCESS) {
            // Handle error
            return INVALID_PORT;
        }

        return port_req.port;
    }
}

unsigned int sleep(unsigned int seconds) {
    if (sleep_reply_port == INVALID_PORT) {
        ports_request_t port_request = create_port(PID_SELF, 0);
        if (port_request.result != SUCCESS) {
            errno = port_request.result;
            return seconds;
        }
        sleep_reply_port = port_request.port;
    }

    size_t ms = seconds * 1000;

    IPC_Start_Timer tmr = {
        .type = IPC_Start_Timer_NUM,
        .ms = ms,
        .reply_port = sleep_reply_port,
    };

    pmos_port_t sleep_port = get_sleep_port();
    if (sleep_port == INVALID_PORT) {
        // Errno is set by get_sleep_port
        // errno = ENOSYS;
        return seconds;
    }

    result_t result = send_message_port(sleep_port, sizeof(tmr), (char*)&tmr);
    if (result != SUCCESS) {
        errno = result;
        return seconds;
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
        return seconds;
    }

    return 0;
}