#include <assert.h>
#include <inttypes.h>
#include <pmos/helpers.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int default_handler(Message_Descriptor *desc, void *message, pmos_right_t *reply_right, pmos_right_t *other,
                           void * ctx, struct pmos_msgloop_data *data)
{
    (void)ctx;
    (void)message;
    (void)reply_right;
    (void)other;

    fprintf(stderr,
            "pmOS libc: Error: Did not find callback for message in port %" PRIu64
            ", sender %" PRIu64 ", right %" PRIu64 ", size %" PRIu64 "\n",
            data->port, desc->sender, desc->sent_with_right, desc->size);

    return PMOS_MSGLOOP_CONTINUE;
}

void pmos_msgloop_initialize(struct pmos_msgloop_data *data, pmos_port_t port)
{
    assert(data);
    assert(port);

    data->port         = port;
    data->default_node = NULL;
    data->nodes        = pmos_msgloop_tree_INITIALIZER;
}

void pmos_msgloop_insert(struct pmos_msgloop_data *data, pmos_msgloop_tree_node_t *node)
{
    assert(data);
    assert(node);

    if (node->data.right_id != 0) {
        pmos_msgloop_tree_insert(&data->nodes, node);
    } else {
        data->default_node = node;
    }
}

int pmos_msgloop_compare(uint64_t *a, uint64_t *b)
{
    if (*a < *b) {
        return -1;
    } else if (*a > *b) {
        return 1;
    } else {
        return 0;
    }
}

void pmos_msgloop_node_set(pmos_msgloop_tree_node_t *n, pmos_right_t right_id,
                           pmos_msgloop_callback_t callback, void *ctx)
{
    assert(n);
    n->data.right_id = right_id;
    n->data.callback = callback;
    n->data.ctx      = ctx;
}

pmos_msgloop_tree_node_t *pmos_msgloop_get(struct pmos_msgloop_data *data, pmos_right_t right)
{
    if (right != 0)
        return pmos_msgloop_tree_find(&data->nodes, &right);
    else
        return data->default_node;
}

void pmos_msgloop_loop(struct pmos_msgloop_data *data)
{
    assert(data);
    bool cont = true;
    while (cont) {
        Message_Descriptor descr;
        result_t result = syscall_get_message_info(&descr, data->port, 0);
        if (result != SUCCESS) {
            fprintf(stderr,
                    "pmOS libc: failed to get message in pmos_msgloop_loop() from port %" PRIu64
                    " error %i\n",
                    data->port, (int)-result);
            break;
        }

        bool have_rights = descr.other_rights_count;
        pmos_right_t rights[4] = {0};
        if (have_rights) {
            result_t result = accept_rights(data->port, rights);
            if (result != SUCCESS) {
                fprintf(stderr,
                    "pmOS libc: failed to accept rights in pmos_msgloop_loop() from port %" PRIu64
                    " error %i\n",
                    data->port, (int)-result);
                break;
            }
        }

        char buff[descr.size];

        right_request_t message = get_first_message(buff, 0, data->port);
        if (message.result != SUCCESS) {
            fprintf(stderr,
                    "pmOS libc: failed to get message in pmos_msgloop_loop() from port %" PRIu64
                    " error %i\n",
                    data->port, (int)-result);

            for (int i = 0; i < 4; ++i) {
                if (rights[i])
                    delete_right(rights[i]);
            }
            break;
        }

        pmos_right_t reply_right = message.right;

        pmos_msgloop_tree_node_t *node = NULL;
        if (descr.sent_with_right != 0)
            node = pmos_msgloop_get(data, descr.sent_with_right);
        if (!node)
            node = data->default_node;

        pmos_msgloop_callback_t callback;
        void *ctx = NULL;

        if (node) {
            callback = node->data.callback;
            ctx      = node->data.ctx;
        } else {
            callback = default_handler;
        }

        int status = callback(&descr, buff, &reply_right, rights, ctx, data);

        if (reply_right != 0)
            delete_right(reply_right);
        if (have_rights)
            for (int i = 0; i < 4; ++i)
                if (rights[i])
                    delete_right(rights[i]);

        cont = status == PMOS_MSGLOOP_CONTINUE;
    }
}
