#include <alloca.h>
#include <errno.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdlib.h>
#include <string.h>

pmos_port_t prepare_reply_port();

result_t name_port(pmos_port_t portnum, const char *name, size_t length, uint32_t flags)
{
    result_t result            = 0;
    IPC_Generic_Msg *reply_msg = NULL;

    pmos_port_t reply_port = prepare_reply_port();
    if (reply_port == INVALID_PORT) {
        return -ENOMEM;
    }

    IPC_Name_Port *n = alloca(sizeof(*n) + length);
    n->type          = IPC_Name_Port_NUM;
    n->flags         = flags;
    n->port          = portnum;
    n->reply_port    = reply_port;
    memcpy(n->name, name, length);

    result_t k_result = send_message_port(0, sizeof(*n) + length, (const char *)n);
    if (k_result != SUCCESS) {
        result = k_result;
        goto out;
    }

    Message_Descriptor reply_descr;
    k_result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port);
    if (k_result != SUCCESS) {
        result = k_result;
        goto out;
    }

    if (reply_descr.size < sizeof(IPC_Name_Port_Reply)) {
        result = -EIO;
        goto out;
    }

    if (reply_msg->type != IPC_Name_Port_Reply_NUM) {
        result = -EIO;
        goto out;
    }

    IPC_Name_Port_Reply *reply = (IPC_Name_Port_Reply *)reply_msg;

    result = reply->result;
out:
    free(reply_msg);
    return result;
}

result_t request_named_port(const char *name, size_t length, pmos_port_t reply_port, unsigned flags)
{
    size_t size = sizeof(IPC_Get_Named_Port) + length;

    IPC_Get_Named_Port *n = alloca(size);
    n->type               = IPC_Get_Named_Port_NUM;
    n->flags              = flags;
    n->reply_port         = reply_port;
    memcpy(n->name, name, length);

    return send_message_port(0, size, (const char *)n);
}

ports_request_t get_port_by_name(const char *name, size_t length, uint32_t flags)
{
    pmos_port_t reply_port = prepare_reply_port();
    if (reply_port == INVALID_PORT) {
        return (ports_request_t) {
            .result = -ENOMEM,
            .port   = 0,
        };
    }

    result_t result = request_named_port(name, length, reply_port, flags);
    if (result)
        return (ports_request_t) {
            .result = result,
            .port   = 0,
        };

    Message_Descriptor reply_descr;
    void *reply_msg;
    result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port);
    if (result != SUCCESS) {
        return (ports_request_t) {
            .result = result,
            .port   = 0,
        };
    }

    if (reply_descr.size < sizeof(IPC_Kernel_Named_Port_Notification)) {
        free(reply_msg);
        return (ports_request_t) {
            .result = -EIO,
            .port   = 0,
        };
    }

    if (((IPC_Generic_Msg *)reply_msg)->type != IPC_Kernel_Named_Port_Notification_NUM) {
        free(reply_msg);
        return (ports_request_t) {
            .result = -EIO,
            .port   = 0,
        };
    }

    IPC_Kernel_Named_Port_Notification *reply = reply_msg;

    ports_request_t rresult = {
        .result = reply->result,
        .port   = reply->port_num,
    };

    free(reply_msg);
    return rresult;
}