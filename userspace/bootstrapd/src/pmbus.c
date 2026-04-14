#include <stdint.h>
#include "pmbus.h"
#include "io.h"
#include "init/init.h"
#include <pmos/pmbus_object.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include "init/init.h"

static bool pmbus_request(struct HookedService *hs);
static bool pmbus_publish(struct PmbusPublishHook *s);

static uint64_t pmbus_right = 0;

struct HookedService {
    struct Service *service;
    uint64_t object_id;
    uint64_t next_bus_id;

    void *filter;

    union {
        struct HookedService *waiting_next;
        pmos_msgloop_tree_node_t pmbus_reply_node;
    };
};

struct HookedService *waiting_for_pmbus = NULL;

struct PmbusPublishHook {
    struct Service *service;
    union {
        struct PmbusPublishHook *waiting_next;
        pmos_msgloop_tree_node_t pmbus_reply_node;
    };
};

struct PmbusPublishHook *waiting_to_publish = NULL;

extern uint64_t loader_port;
extern struct pmos_msgloop_data msgloop_data;

static int pmbus_reply_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void * ctx, struct pmos_msgloop_data *data)
{
    (void)reply_right;

    print_str("Loader: Received PMBUS reply for service with object id ");
    print_hex(desc->sent_with_right);
    print_str("\n");

    struct HookedService *hs = ctx;
    bool retry = hs->service->run_type == RUN_FOR_EACH_MATCH;

    IPC_BUS_Request_Object_Reply *r = buff;
    if (desc->size < sizeof(*r)) {
        print_str("Loader: recieved too small of a message for device request\n");
        retry = false;
    } else if (r->result != 0) {
        print_str("Loader: error in recieving a pmbus object...");
        retry = true;
    } else if (!extra_rights[0]) {
        hs->next_bus_id = r->next_sequence_number;
        print_str("Loader: recieved pmbus object with no right (probably expired...)\n");
        retry = true;
    } else {
        hs->next_bus_id = r->next_sequence_number;

        print_str("Starting service ");
        if (hs->service->name)
            print_str(hs->service->name);
        else
            print_str("<UNKNOWN>");

        print_str(" for pmbus object ID ");
        print_hex(r->object_id);
        print_str("\n");

        int result = start_service(hs->service, hs->object_id, extra_rights[0]);
        if (result) {
            print_str("Failed to start service: ");
            print_hex(-result);
            print_str("\n");
            retry = true;
        } else {
            extra_rights[0] = 0;
        }
    }

    pmos_msgloop_erase(data, &hs->pmbus_reply_node);
    if (retry)
        pmbus_request(hs);

    return 0;
}

static int pmbus_publish_reply_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void * ctx, struct pmos_msgloop_data *data)
{
    (void)reply_right;
    (void)extra_rights;

    struct PmbusPublishHook *s = ctx;

    IPC_BUS_Publish_Object_Reply *r = buff;
    if (desc->size < sizeof(*r)) {
        print_str("Loader: Recieved too small message for object publish\n");
    } else if (r->result != 0) {
        print_str("Loader: error in sending a pmbus object. Error: ");
        print_hex(r->result);
        print_str("\n");
    } else {
        print_str("Published pmbus object for the service ");
        const char *name = s->service->name;
        if (!name)
            name = "UNKNOWN";

        print_str(name);
        print_str(", sequence number: ");
        print_hex(r->sequence_number);
        print_str("\n");
    }

    pmos_msgloop_erase(data, &s->pmbus_reply_node);

    return 0;
}


static bool pmbus_request(struct HookedService *hs)
{
    size_t size = pmos_bus_filter_serialize_ipc(hs->filter, NULL);
    if (size == 0) {
        print_str("Loader: Failed to serialize filter for service ");
        print_str(hs->service->name);
        print_str("\n");
        return false;
    }

    uint64_t data[(size + sizeof(IPC_BUS_Request_Object))/8];
    IPC_BUS_Request_Object *req = (IPC_BUS_Request_Object *)data;
    req->type = IPC_BUS_Request_Object_NUM;
    req->flags = 0;
    req->start_sequence_number = hs->next_bus_id;

    pmos_bus_filter_serialize_ipc(hs->filter, (uint8_t *)data + sizeof(IPC_BUS_Request_Object));

    auto result = send_message_right(pmbus_right, loader_port, req, sizeof(data), NULL, 0);
    if (result.result) {
        print_str("Loader: Failed to send PMBUS request for service ");
        print_str(hs->service->name);
        print_str(". Error: ");
        print_hex(result.result);
        print_str("\n");
        return false;
    }

    pmos_msgloop_node_set(&hs->pmbus_reply_node, result.right, pmbus_reply_callback, hs);
    pmos_msgloop_insert(&msgloop_data, &hs->pmbus_reply_node);

    return true;
}

static bool pmbus_publish(struct PmbusPublishHook *s)
{
    struct Service *service = s->service;
    pmos_bus_object_t *object = NULL;
    uint8_t *ipc_data = NULL;
    bool result = true;
    right_request_t right = {};

    object = construct_pmbus_object(service);
    if (!object) {
        print_str("Loader: Failed to construct the pmbus object for a service...\n");
        result = false;
        goto end;
    }

    size_t size = 0;
    result = pmos_bus_object_serialize_ipc(object, &ipc_data, &size);
    if (!result) {
        print_str("Loader: Failed to serialize pmbus object into IPC...\n");
        goto end;
    }

    right = dup_right(service->service_right);
    if (right.result) {
        print_str("Loader: Failed to dup right, error ");
        print_hex(right.result);
        print_str("\n");
        right = (right_request_t){};
        result = false;
        goto end;
    }

    message_extra_t extra = {
        .extra_rights = {right.right, },
    };

    auto send_result = send_message_right(pmbus_right, loader_port, ipc_data, size, &extra, 0);
    if (send_result.result) {
        print_str("Loader: Failed to send PMBUS request for service ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(send_result.result);
        print_str("\n");
        result = false;
        goto end;
    }
    // The new right above has been consumed by the send syscall
    right = (right_request_t){};

    pmos_msgloop_node_set(&s->pmbus_reply_node, send_result.right, pmbus_publish_reply_callback, s);
    pmos_msgloop_insert(&msgloop_data, &s->pmbus_reply_node);

end:
    if (right.right)
        delete_right(right.right);
    free(ipc_data);
    pmos_bus_object_free(object);
    return result;
}

void free_hooked_service(struct HookedService *hs)
{
    if (!hs)
        return;

    pmos_bus_filter_free(hs->filter);
    free(hs);
}

void hook_match_service(struct Service *service, uint64_t object_id)
{
    void *filter = construct_filter(service);
    if (!filter) {
        print_str("Loader: Could not construct filter for service ");
        print_str(service->name);
        print_str("\n");
        return;
    }

    struct HookedService *hs = calloc(1, sizeof(struct HookedService));
    if (!hs) {
        print_str("Loader: Could not allocate memory for hooked service ");
        print_str(service->name);
        print_str("\n");
        pmos_bus_filter_free(filter);
        return;
    }

    hs->service = service;
    hs->object_id = object_id;
    hs->filter = filter;

    service->hook = hs;

    print_str("Loader: Hooking up match service ");
    print_str(service->name);
    print_str("\n");

    if (!pmbus_right) {
        hs->waiting_next = waiting_for_pmbus;
        waiting_for_pmbus = hs;
    } else {
        bool result = pmbus_request(hs);
        if (!result) {
            print_str("Loader: Failed to request PMBUS for service ");
            print_str(service->name);
            print_str("\n");
            free_hooked_service(hs);
        }
    }
}

void pmbus_callback(int result, const char * /* right_name */, pmos_right_t right)
{
    if (result) {
        print_str("Loader: Failed to get PMBUS port. Error: ");
        print_hex(result);
        print_str("\n");
        return;
    }

    pmbus_right = right;

    struct HookedService *hs = waiting_for_pmbus;
    while (hs) {
        struct HookedService *next = hs->waiting_next;

        bool result = pmbus_request(hs);
        if (!result) {
            print_str("Loader: Failed to request PMBUS for service ");
            print_str(hs->service->name);
            print_str("\n");
            free_hooked_service(hs);
        }

        hs = next;
    }
    waiting_for_pmbus = NULL;

    struct PmbusPublishHook *ss = waiting_to_publish;
    while (ss) {
        struct PmbusPublishHook *next = ss->waiting_next;
        bool result = pmbus_publish(ss);
        if (!result) {
            print_str("Loader: Failed to publish pmbus object for service ");
            print_str(ss->service->name);
            print_str("\n");
            // free_hooked_service(ss);
        }

        ss = next;
    }
    waiting_to_publish = NULL;
}

extern struct Service *services;

void publish_services()
{
    for (struct Service *s = services; s; s = s->next) 
        publish_service(s);
}

bool create_service_right(struct Service *service)
{
    if (service->service_recieve_right)
        return true;

    uint64_t recieve_right;
    right_request_t right = create_right(loader_port, &recieve_right, 0);
    if (right.result != SUCCESS) {
        print_str("Loader: failed to create right for a service: ");
        print_hex(right.result);
        print_str("\n");
        return false;
    }

    service->service_recieve_right = recieve_right;
    service->service_right = right.right;

    return true;
}

void publish_service(struct Service *service)
{
    if (!create_service_right(service))
        return;

    struct PmbusPublishHook *s = NULL;
    if (service->publish_hook)
        return;

    s = calloc(1, sizeof(*s));
    if (!s) {
        print_str("Loader: Failed to allocate memory for a service...\n");
        goto error;
    }

    s->service = service;    

    if (!pmbus_right) {
        s->waiting_next = waiting_to_publish;
        waiting_to_publish = s;
    } else {
        bool result = pmbus_publish(s);
        if (!result) {
            print_str("Loader: Failed to publish PMBUS object for service ");
            print_str(service->name);
            print_str("\n");
            goto error;
        }
    }

    service->publish_hook = s;
    s = NULL;
error:
    free(s);
}