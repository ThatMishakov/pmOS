#include <alloca.h>
#include <errno.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdlib.h>
#include <string.h>

pmos_port_t prepare_reply_port();

result_t name_right(pmos_right_t right, const char *name, size_t length, uint32_t flags)
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
    memcpy(n->name, name, length);

    message_extra_t extra = {
        .extra_rights = {right},
    };

    right_request_t s_result = send_message_right(0, reply_port, n, sizeof(*n) + length, &extra, 0);
    if (s_result.result != SUCCESS) {
        result = s_result.result;
        goto out;
    }

    Message_Descriptor reply_descr;
    result_t k_result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port, NULL, NULL);
    if (k_result != SUCCESS) {
        delete_right(s_result.right);
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

right_request_t request_named_port(const char *name, size_t length, pmos_port_t reply_port, unsigned flags)
{
    size_t size = sizeof(IPC_Get_Named_Port) + length;

    IPC_Get_Named_Port *n = alloca(size);
    n->type               = IPC_Get_Named_Port_NUM;
    n->flags              = flags;
    memcpy(n->name, name, length);

    return send_message_right(0, reply_port, n, size, 0, 0);
}

right_request_t get_right_by_name(const char *name, size_t length, uint32_t flags)
{
    pmos_port_t reply_port = prepare_reply_port();
    if (reply_port == INVALID_PORT) {
        return (right_request_t) {
            .result = -ENOMEM,
            .right   = 0,
        };
    }

    right_request_t r_result = request_named_port(name, length, reply_port, flags);
    if (r_result.result)
        return (right_request_t) {
            .result = r_result.result,
            .right   = 0,
        };

    Message_Descriptor reply_descr;
    void *reply_msg;
    pmos_right_t rights[4] = {0};
    result_t result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port, NULL, rights);
    if (result != SUCCESS) {
        delete_right(r_result.right);
        return (right_request_t) {
            .result = result,
            .right   = 0,
        };
    }

    if (reply_descr.size < sizeof(IPC_Kernel_Named_Port_Notification)) {
        free(reply_msg);
        return (right_request_t) {
            .result = -EIO,
            .right   = 0,
        };
    }

    if (((IPC_Generic_Msg *)reply_msg)->type != IPC_Kernel_Named_Port_Notification_NUM) {
        free(reply_msg);
        return (right_request_t) {
            .result = -EIO,
            .right   = 0,
        };
    }

    IPC_Kernel_Named_Port_Notification *reply = reply_msg;

    right_request_t rresult = {
        .result = reply->result,
        .right   = rights[0],
    };

    free(reply_msg);
    return rresult;
}