#include <errno.h>
#include <pmos/pmbus_helper.h>
#include <stdlib.h>
#include <string.h>
#include <pmos/ipc.h>
#include <inttypes.h>

static const char *PMBUS_NAME = "/pmos/pmbus";

struct object_request {
    struct object_request *ll_prev, *ll_next;
    struct pmbus_helper *helper;

    pmos_bus_object_t *object;
    pmos_right_t object_right_owning;
    pmbus_helper_callback_t callback;
    void *callback_ctx;

    pmos_msgloop_tree_node_t node;
    pmos_right_t reply_right;
};

struct pmbus_helper {
    struct pmos_msgloop_data *for_msgloop;
    pmos_right_t pmbus_right;

    pmos_right_t pmbus_reply_right;
    pmos_msgloop_tree_node_t request_node;
    // Linked list of requests, while waiting for the right...
    struct object_request *next_request;

    // Pending register reply
    struct object_request *next_pending;
};

static void ll_push(struct object_request **head, struct object_request *node)
{
    node->ll_next = *head;
    if (*head)
        node->ll_next->ll_prev = node;
    node->ll_prev = NULL;
    *head = node; 
}

static void ll_delete(struct object_request **head, struct object_request *node)
{
    if (node->ll_prev) {
        node->ll_prev->ll_next = node->ll_next;
    } else {
        *head = node->ll_next;
    }

    if (node->ll_next)
        node->ll_next->ll_prev = node->ll_prev;

    node->ll_next = node->ll_prev = NULL;
}

struct pmbus_helper *pmbus_helper_create(struct pmos_msgloop_data *for_msgloop)
{
    if (!for_msgloop)
        return NULL;

    struct pmbus_helper *h = calloc(1, sizeof(*h));
    if (!h)
        return NULL;

    h->for_msgloop = for_msgloop;
    return h;
}

static void free_object_request(struct object_request *r)
{
    if (!r)
        return;

    pmos_bus_object_free(r->object);
    delete_right(r->object_right_owning);

    if (r->reply_right) {
        pmos_msgloop_erase(r->helper->for_msgloop, &r->node);
        delete_receive_right(r->helper->for_msgloop->port, r->reply_right);
    }
    free(r);
}

static int send_pmbus_object(struct pmbus_helper *helper, struct object_request *req_owning);

static void send_objects(struct pmbus_helper *helper)
{
    while (helper->next_request) {
        struct object_request *r = helper->next_request;
        ll_delete(&helper->next_request, r);

        pmbus_helper_callback_t callback = r->callback;
        void *ctx = r->callback_ctx;

        int result = send_pmbus_object(helper, r);
        if (result > 0)
            break;
        else if (result < 0) {
            if (callback)
                callback(result, 0, ctx, helper);
        }
    }
}

static int request_pmbus_right(struct pmbus_helper *helper);

static int right_request_callback(Message_Descriptor *desc, void *message, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void *ctx, struct pmos_msgloop_data *data)
{
    struct pmbus_helper *helper = ctx;
    pmos_msgloop_erase(data, &helper->request_node);
    helper->pmbus_reply_right = 0;
    (void)reply_right;

    if (desc->size < IPC_MIN_SIZE) {
        fprintf(stderr, "pmbus helper: Recieved too small message %" PRIu64 ", expecting reply to named port request\n", desc->size);
        return PMOS_MSGLOOP_CONTINUE;
    }

    if (IPC_TYPE(message) != IPC_Kernel_Named_Port_Notification_NUM) {
        fprintf(stderr, "pmbus helper: Received wrong message type for pmbus right request: %" PRIu32 "\n", IPC_TYPE(message));
        return PMOS_MSGLOOP_CONTINUE;
    }

    IPC_Kernel_Named_Port_Notification *n = message;

    if (n->result) {
        fprintf(stderr, "Error getting pmbus right: %" PRIi32 "\n", n->result);
        return PMOS_MSGLOOP_CONTINUE;
    }

    pmos_right_t right = extra_rights[0];
    if (right == 0) {
        // pmbus server died...
        request_pmbus_right(helper);
        return PMOS_MSGLOOP_CONTINUE;
    }

    helper->pmbus_right = right;

    send_objects(helper);

    return PMOS_MSGLOOP_CONTINUE;
}

static int send_object_callback(Message_Descriptor *desc, void *message, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void *ctx, struct pmos_msgloop_data *data)
{
    (void)reply_right;
    (void)extra_rights;

    struct object_request *req = ctx;
    pmos_msgloop_erase(data, &req->node);
    struct pmbus_helper *helper = req->helper;
    ll_delete(&helper->next_pending, req);

    pmbus_helper_callback_t callback = req->callback;
    void *callback_ctx = req->callback_ctx;
    free_object_request(req);

    int result = 0;
    uint64_t object_id = 0;

    if (desc->size < IPC_MIN_SIZE) {
        fprintf(stderr, "pmbus helper: Recieved too small message %" PRIu64 ", expecting reply to send object\n", desc->size);
        result = -EINTR;
        goto end;
    }

    if (IPC_TYPE(message) != IPC_BUS_Publish_Object_Reply_NUM) {
        fprintf(stderr, "pmbus helper: Received wrong message type for pmbus object publish: %" PRIu32 "\n", IPC_TYPE(message));
        result = -EINTR;
        goto end;
    }

    IPC_BUS_Publish_Object_Reply *r = message;
    result = r->result;
    object_id = r->sequence_number;

end:
    if (callback)
        callback(result, object_id, callback_ctx, helper);

    return PMOS_MSGLOOP_CONTINUE;
}

static int request_pmbus_right(struct pmbus_helper *helper)
{
    if (helper->pmbus_reply_right)
        return 0;

    right_request_t reply_right =
        request_named_port(PMBUS_NAME, strlen(PMBUS_NAME), helper->for_msgloop->port, 0);
    if ((int)reply_right.result)
        return (int)reply_right.result;

    helper->pmbus_reply_right = reply_right.right;

    pmos_msgloop_node_set(&helper->request_node, reply_right.right, right_request_callback, helper);
    pmos_msgloop_insert(helper->for_msgloop, &helper->request_node);
    return 0;
}

static int request_right_and_push(struct pmbus_helper *helper, struct object_request *req_owning)
{
    int result = request_pmbus_right(helper);

    if (result == 0) {
        ll_push(&helper->next_request, req_owning);
        req_owning           = NULL;
    }

    free_object_request(req_owning);

    return result;
}

static int send_pmbus_object(struct pmbus_helper *helper, struct object_request *req_owning)
{
    size_t size = 0;
    uint8_t *data_out = NULL;
    int result = 0;

    if (!pmos_bus_object_serialize_ipc(req_owning->object, &data_out, &size)) {
        result = -ENOMEM;
        goto end;
    }

    message_extra_t extra = {
        .extra_rights = {
            [0] = req_owning->object_right_owning,
        },
    };

    right_request_t reply_right =
        send_message_right(helper->pmbus_right, helper->for_msgloop->port, data_out, size, &extra, 0);
    if (reply_right.result) {
        if ((int)reply_right.result == -ENOENT) {
            // pmbus right closed...
            helper->pmbus_right = 0;
            result = request_right_and_push(helper, req_owning);
            req_owning = NULL;
            result = 1;
        } else {
            result = reply_right.result;
        }
        goto end;
    }

    req_owning->object_right_owning = 0;
    req_owning->reply_right = reply_right.right;
    pmos_msgloop_node_set(&req_owning->node, reply_right.right, send_object_callback, req_owning);
    pmos_msgloop_insert(helper->for_msgloop, &req_owning->node);

    ll_push(&helper->next_pending, req_owning);
    req_owning = NULL;

end:
    free_object_request(req_owning);
    free(data_out);
    return result;
}

int pmbus_helper_publish(struct pmbus_helper *helper, pmos_bus_object_t *object_owning,
                         pmos_right_t object_right_owning, pmbus_helper_callback_t callback,
                         void *callback_ctx)
{
    struct object_request *req = NULL;
    int result                 = 0;

    if (!helper) {
        result = -EINVAL;
        goto end;
    }

    if (!object_owning) {
        result = -EINVAL;
        goto end;
    }

    if (object_right_owning == 0) {
        result = -EINVAL;
        goto end;
    }

    req = calloc(1, sizeof(*req));
    if (!req) {
        result = -ENOMEM;
        goto end;
    }

    req->callback            = callback;
    req->callback_ctx        = callback_ctx;
    req->object              = object_owning;
    req->object_right_owning = object_right_owning;
    req->helper              = helper;
    object_owning            = NULL;
    object_right_owning      = 0;

    if (!helper->pmbus_right) {
        result = request_right_and_push(helper, req);
        req = NULL;
    } else {
        result = send_pmbus_object(helper, req);
        if (result > 0)
            result = 0;
        req    = NULL;
    }

end:
    free_object_request(req);
    delete_right(object_right_owning);
    pmos_bus_object_free(object_owning);
    return result;
}