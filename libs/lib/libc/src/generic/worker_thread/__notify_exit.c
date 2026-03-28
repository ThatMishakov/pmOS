#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/tls.h>

extern pmos_port_t worker_port;

__attribute__((visibility("hidden"))) void __notify_exit(void *exit_code, int type)
{
    IPC_Exit msg = {
        .type   = IPC_Exit_NUM,
        .exit_type = type,
        .exit_code = exit_code,
    };

    send_message_port(worker_port, sizeof(msg), &msg);
}