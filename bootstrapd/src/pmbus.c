#include <stdint.h>
#include "pmbus.h"
#include "io.h"
#include "init/init.h"
#include <pmos/pmbus_object.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include "init/init.h"

static bool pmbus_request(struct HookedService *hs);

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

extern uint64_t loader_port;
extern struct pmos_msgloop_data msgloop_data;

static int pmbus_reply_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void * ctx, struct pmos_msgloop_data *data)
{
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
}