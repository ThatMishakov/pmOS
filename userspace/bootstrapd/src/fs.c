#include "fs.h"
#include <alloca.h>
#include <pmos/system.h>
#include <pmos/ipc.h>
#include "io.h"
#include <string.h>
#include <pmos/pmbus_object.h>
#include <pmos/helpers.h>

extern pmos_right_t posix_server_right;
extern uint64_t loader_port;
extern pmos_port_t request_port;

extern struct pmos_msgloop_data msgloop_data;
static pmos_right_t fs_right;

const char *rootfs_path = "/";
const uint64_t rootfs_inode = 1;

static pmos_msgloop_tree_node_t fs_node;

static int filesystem_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                              pmos_right_t *extra_rights, void *ctx, struct pmos_msgloop_data *data)
{
    (void)reply_right;
    (void)extra_rights;
    (void)ctx;
    (void)data;

    if (desc->size < sizeof(IPC_Generic_Msg)) {
        print_str("Loader: Received very small message from filesystem\n");
        return -1;
    }

    IPC_Generic_Msg *ipc_msg = (IPC_Generic_Msg *)(buff);
    switch (ipc_msg->type) {
    case IPC_Kernel_Receive_Right_Destroyed_NUM:
        print_str("Loader: Received IPC_Kernel_Receive_Right_Destroyed from filesystem\n");
        pmos_msgloop_erase(data, &fs_node);
        // TODO: This is a good place to re-mount as root (if the posix server restarts)
        break;

    default:
        print_str("Loader: Unknown message type ");
        print_hex(ipc_msg->type);
        print_str(" from filesystem\n");
        break;
    }

    return 0;
}

void mount_as_root()
{
    pmos_right_t vfs_right = INVALID_RIGHT;
    void *message = nullptr;

    if (posix_server_right == INVALID_RIGHT) {
        print_str("Loader: posix server right is invalid, cannot mount as root\n");
        goto error;
    }

    auto ret = create_right(loader_port, &fs_right, 0);
    if (ret.result != SUCCESS) {
        print_str("Loader: failed to create filesystem right: ");
        print_hex(ret.result);
        print_str("\n");
        goto error;
    }
    vfs_right = ret.right;

    pmos_msgloop_node_set(&fs_node, fs_right, filesystem_callback, NULL);
    pmos_msgloop_insert(&msgloop_data, &fs_node);

    size_t str_len = strlen(rootfs_path);
    size_t struct_len = sizeof(IPC_Mount_FS) + str_len;
    IPC_Mount_FS *msg = alloca(struct_len);

    msg->num = IPC_Mount_FS_NUM;
    msg->flags = 0;
    msg->root_fd = rootfs_inode;
    memcpy(msg->mount_path, rootfs_path, str_len);

    message_extra_t rights = {
        .extra_rights = {vfs_right}
    };

    right_request_t reply_right = send_message_right(posix_server_right, request_port, msg, struct_len, &rights, 0);
    if (reply_right.result != SUCCESS) {
        print_str("Loader: failed to send mount request to posix server: ");
        print_hex(reply_right.result);
        print_str("\n");
        goto error;
    }
    vfs_right = INVALID_RIGHT; // The right has been sent away

    Message_Descriptor desc;
    result_t msg_result = get_message(&desc, (unsigned char **)&message, request_port, NULL, NULL);
    if (msg_result) {
        print_str("Loader: Failed to get reply message from posix server: ");
        print_hex(msg_result);
        print_str("\n");
        goto error;
    }

    if (desc.size < sizeof(IPC_Generic_Msg)) {
        print_str("Loader: Received very small message from posix server\n");
        goto error;
    }

    IPC_Generic_Msg *reply_msg = (IPC_Generic_Msg *)(message);
    if (reply_msg->type != IPC_Mount_FS_Reply_NUM) {
        print_str("Loader: Received unexpected message type from posix server: ");
        print_hex(reply_msg->type);
        print_str("\n");
        goto error;
    }

    IPC_Mount_FS_Reply *reply = (IPC_Mount_FS_Reply *)(message);
    if (reply->result_code != 0) {
        print_str("Loader: Received error code from posix server: ");
        print_hex(reply->result_code);
        print_str("\n");
        goto error;
    }

    print_str("Loader: Mounted filesystem as root successfully\n");
error:
    if (message)
        free(message);
    if (vfs_right != INVALID_RIGHT) {
        delete_right(vfs_right);
    }
    return;
}