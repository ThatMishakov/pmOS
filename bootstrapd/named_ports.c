#include "named_ports.h"

#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <pmos/containers/rbtree.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct CallBackNode {
    struct CallBackNode *next, *prev;
    union {
        pmos_port_t reply_port;
        register_callback_t callback;
    };

    bool is_callback;
};

struct NamedPort {
    char *string;
    pmos_port_t port;
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
        node->prev = NULL;
    } else {
        node->prev = port->callbacks_tail;
        port->callbacks_tail->next = node;
        port->callbacks_tail = node;
    }
}

static void do_callback(struct CallBackNode *node, struct NamedPort *port)
{
    assert(node);
    assert(port);

    if (node->is_callback) {
        node->callback(0, port->string, port->port);
    } else {
        auto name_length = strlen(port->string);
        IPC_Kernel_Named_Port_Notification *n = alloca(sizeof(*n) + name_length);

        n->type     = IPC_Kernel_Named_Port_Notification_NUM;
        n->result   = 0;
        n->port_num = port->port;

        memcpy(n->port_name, port->string, name_length);

        auto result = send_message_port(node->reply_port, sizeof(*n) + name_length, n);
        if (result) {
            // TODO: Print error
            (void)result;
        }
    }
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

int register_port(const char *name, size_t name_length, pmos_port_t port)
{
    int result              = 0;
    char *new_name          = NULL;
    ports_tree_node_t *node = NULL;

    struct BoundedString str = {
        .str    = name,
        .length = name_length,
    };

    auto t_node = ports_tree_find(&tree, &str);
    if (t_node) {
        if (t_node->data.port != 0) {
            result = -EEXIST;
            goto error;
        }

        t_node->data.port = port;

        struct CallBackNode *callback;
        while ((callback = t_node->data.callbacks_head)) {
            callback_erase(&t_node->data, callback);
            do_callback(callback, &t_node->data);
            free(callback);
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
        node->data.port = port;
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
        if (t_node->data.port) {
            callback(0, t_node->data.string, t_node->data.port);
            return 0;
        } else {
            struct CallBackNode *cnode = calloc(sizeof(*cnode), 1);
            if (!cnode)
                return -ENOMEM;
            
            cnode->is_callback = true;
            cnode->callback = callback;

            callback_insert(&t_node->data, cnode);

            return 0;
        }
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
        cnode->callback = callback;

        callback_insert(&node->data, cnode);

        return 0;
    error:
        free(cnode);
        free(node);
        free(new_name);
        return result;
    }
}

static void name_port_reply(pmos_port_t reply_port, int result, pmos_port_t data_port, const char *name, size_t name_length)
{
    IPC_Kernel_Named_Port_Notification *n = alloca(sizeof(*n) + name_length);

    n->type     = IPC_Kernel_Named_Port_Notification_NUM;
    n->result   = result;
    n->port_num = data_port;

    memcpy(n->port_name, name, name_length);

    auto r = send_message_port(reply_port, sizeof(*n) + name_length, n);
    if (r) {
        // TODO: Print error
        (void)r;
    }
}

void request_port_message(const char *name, size_t name_length, int flags, pmos_port_t reply_port)
{
    struct BoundedString str = {
        .str    = name,
        .length = name_length,
    };

    auto t_node = ports_tree_find(&tree, &str);
    if (t_node) {
        if (t_node->data.port) {
            name_port_reply(reply_port, 0, t_node->data.port, name, name_length);
        } else {
            struct CallBackNode *cnode = calloc(sizeof(*cnode), 1);
            if (!cnode) {
                name_port_reply(reply_port, -ENOMEM, 0, name, name_length);
                return;
            }

            
            cnode->is_callback = false;
            cnode->reply_port = reply_port;

            callback_insert(&t_node->data, cnode);
        }
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
        cnode->reply_port = reply_port;

        callback_insert(&node->data, cnode);

        return;
    error:
        free(cnode);
        free(node);
        free(new_name);
        name_port_reply(reply_port, result, 0, name, name_length);
    }
}   