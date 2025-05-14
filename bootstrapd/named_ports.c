#include "named_ports.h"

#include "io.h"

#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <pmos/containers/rbtree.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct CallBackNode {
    struct CallBackNode *next, *prev;
    union {
        pmos_right_t reply_right;
        register_callback_t callback;
    };

    bool is_callback;
};

struct NamedPort {
    char *string;
    pmos_right_t right;
    struct CallBackNode *callbacks_head, *callbacks_tail;
};

static void callback_erase(struct NamedPort *port, struct CallBackNode *node)
{
    if (!port || !node)
        return;

    if (node->prev)
        node->prev->next = node->next;
    else
        port->callbacks_head = node->next;

    if (node->next)
        node->next->prev = node->prev;
    else
        port->callbacks_tail = node->prev;
}

static void callback_insert(struct NamedPort *port, struct CallBackNode *node)
{
    assert(port);
    assert(node);

    node->next = NULL;

    if (port->callbacks_head == NULL) {
        assert(!port->callbacks_tail);
        port->callbacks_head = node;
        port->callbacks_tail = node;
        node->prev           = NULL;
    } else {
        node->prev                 = port->callbacks_tail;
        port->callbacks_tail->next = node;
        port->callbacks_tail       = node;
    }
}

static bool /* continue */ do_callback(struct CallBackNode *node, struct NamedPort *port)
{
    assert(node);
    assert(port);

    auto right = dup_right(port->right);
    if (right.result != SUCCESS)
        return false;

    if (node->is_callback) {
        node->callback(0, port->string, right.right);
    } else {
        auto name_length = strlen(port->string);
        auto length      = sizeof(IPC_Kernel_Named_Port_Notification) + name_length;
        IPC_Kernel_Named_Port_Notification *n = alloca(length);

        n->type   = IPC_Kernel_Named_Port_Notification_NUM;
        n->result = 0;

        memcpy(n->port_name, port->string, name_length);

        message_extra_t extra = {
            .extra_rights = {right.right},
        };

        auto result =
            send_message_right(node->reply_right, 0, n, length, &extra, SEND_MESSAGE_DELETE_RIGHT);
        switch (result.result) {
        case -ENOMEM:
            delete_right(right.right);
            delete_right(port->right);
            print_str("Loader: failed to reply with right, no memory");
            return true;
        case -ESRCH:
            if (result.right == 0) {
                delete_right(right.right);
                return true;
            } else {
                return false;
            }
        case 0:
            break;
        default:
            delete_right(right.right);
            delete_right(port->right);
            print_str("Loader: failed to reply with right, unknown error");
            return true;
        }
    }

    return true;
}

static int port_compare(struct NamedPort *a, struct NamedPort *b)
{
    return strcmp(a->string, b->string);
}

struct BoundedString {
    const char *str;
    size_t length;
};

int key_compare(struct NamedPort *a, struct BoundedString *s)
{
    int result = strncmp(a->string, s->str, s->length);
    if (result)
        return result;

    return a->string[s->length];
}

RBTREE(ports_tree, struct NamedPort, port_compare, key_compare)

ports_tree_tree_t tree = ports_tree_INITIALIZER;

int register_right(const char *name, size_t name_length, pmos_right_t right)
{
    if (!right)
        return -EINVAL;

    int result              = 0;
    char *new_name          = NULL;
    ports_tree_node_t *node = NULL;

    struct BoundedString str = {
        .str    = name,
        .length = name_length,
    };

    auto t_node = ports_tree_find(&tree, &str);
    if (t_node) {
        if (t_node->data.right != 0) {
            delete_right(t_node->data.right);
        }

        t_node->data.right = right;

        struct CallBackNode *callback;
        while ((callback = t_node->data.callbacks_head)) {
            if (do_callback(callback, &t_node->data)) {
                callback_erase(&t_node->data, callback);
                free(callback);
            } else {
                break;
            }
        }
    } else {
        new_name = strndup(name, name_length);
        if (!new_name) {
            result = -ENOMEM;
            goto error;
        }

        node = calloc(sizeof(*node), 1);
        if (!node) {
            result = -ENOMEM;
            goto error;
        }

        node->data.string = new_name;
        node->data.right  = right;
        ports_tree_insert(&tree, node);
    }

    return 0;

error:
    free(new_name);
    free(node);
    return result;
}

int request_port_callback(const char *name, size_t name_length, register_callback_t callback)
{
    struct BoundedString str = {
        .str    = name,
        .length = name_length,
    };

    auto t_node = ports_tree_find(&tree, &str);
    if (t_node) {
        if (t_node->data.right) {
            auto right = dup_right(t_node->data.right);
            if (right.result == SUCCESS) {
                callback(0, t_node->data.string, right.result);
                return 0;
            }
        }

        struct CallBackNode *cnode = calloc(sizeof(*cnode), 1);
        if (!cnode)
            return -ENOMEM;

        cnode->is_callback = true;
        cnode->callback    = callback;

        callback_insert(&t_node->data, cnode);

        return 0;
    } else {
        int result                 = 0;
        ports_tree_node_t *node    = NULL;
        struct CallBackNode *cnode = NULL;
        char *new_name             = strndup(name, name_length);
        if (!new_name) {
            result = -ENOMEM;
            goto error;
        }

        node = calloc(sizeof(*node), 1);
        if (!node) {
            result = -ENOMEM;
            goto error;
        }

        cnode = calloc(sizeof(*cnode), 1);
        if (!cnode) {
            result = -ENOMEM;
            goto error;
        }

        node->data.string = new_name;
        ports_tree_insert(&tree, node);

        cnode->is_callback = true;
        cnode->callback    = callback;

        callback_insert(&node->data, cnode);

        return 0;
    error:
        free(cnode);
        free(node);
        free(new_name);
        return result;
    }
}

static void name_port_reply_error(pmos_right_t reply_right, int result, const char *name,
                                  size_t name_length)
{
    auto length = sizeof(IPC_Kernel_Named_Port_Notification) + name_length;
    IPC_Kernel_Named_Port_Notification *n = alloca(length);

    n->type   = IPC_Kernel_Named_Port_Notification_NUM;
    n->result = result;

    memcpy(n->port_name, name, name_length);

    auto r = send_message_right(reply_right, 0, n, length, NULL, SEND_MESSAGE_DELETE_RIGHT);
    if (r.result) {
        // TODO: Print error
        (void)r;
    }
}

void request_port_message(const char *name, size_t name_length, int flags, pmos_right_t reply_right)
{
    if (reply_right == 0) {
        print_str("bootstrapd: requested right with no reply_right!\n");
        return;
    }

    struct BoundedString str = {
        .str    = name,
        .length = name_length,
    };

    auto t_node = ports_tree_find(&tree, &str);
    if (t_node) {
        right_request_t dr;
        if (t_node->data.right && (dr = dup_right(t_node->data.right)).result == SUCCESS) {
            auto length      = sizeof(IPC_Kernel_Named_Port_Notification) + name_length;
            IPC_Kernel_Named_Port_Notification *n = alloca(length);

            n->type   = IPC_Kernel_Named_Port_Notification_NUM;
            n->result = 0;

            memcpy(n->port_name, name, name_length);

            message_extra_t extra = {
                .extra_rights = {dr.right},
            };

            auto result = send_message_right(reply_right, 0, n, length, &extra,
                                             SEND_MESSAGE_DELETE_RIGHT);

            switch (result.result) {
            case -ENOMEM:
                delete_right(dr.right);
                delete_right(reply_right);
                print_str("Loader: failed to reply with right, no memory");
                return;
            case -ESRCH:
                if (result.right == 0) {
                    delete_right(dr.right);
                    return;
                }
                break;
            case 0:
                return;

            default:
                delete_right(dr.right);
                delete_right(reply_right);
                print_str("Loader: failed to reply with right, unknown error");
                return;
            }
        }

        struct CallBackNode *cnode = calloc(sizeof(*cnode), 1);
        if (!cnode) {
            name_port_reply_error(reply_right, -ENOMEM, name, name_length);
            return;
        }

        cnode->is_callback = false;
        cnode->reply_right = reply_right;

        callback_insert(&t_node->data, cnode);
    } else {
        int result                 = 0;
        ports_tree_node_t *node    = NULL;
        struct CallBackNode *cnode = NULL;
        char *new_name             = strndup(name, name_length);
        if (!new_name) {
            result = -ENOMEM;
            goto error;
        }

        node = calloc(sizeof(*node), 1);
        if (!node) {
            result = -ENOMEM;
            goto error;
        }

        cnode = calloc(sizeof(*cnode), 1);
        if (!cnode) {
            result = -ENOMEM;
            goto error;
        }

        node->data.string = new_name;
        ports_tree_insert(&tree, node);

        cnode->is_callback = false;
        cnode->reply_right = reply_right;

        callback_insert(&node->data, cnode);

        return;
    error:
        free(cnode);
        free(node);
        free(new_name);
        name_port_reply_error(reply_right, result, name, name_length);
    }
}