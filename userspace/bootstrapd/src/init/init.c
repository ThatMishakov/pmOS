#include "init.h"
#include <pmos/vector.h>
#include <stdlib.h>
#include <pmos/pmbus_object.h>
#include <string.h>
#include "../io.h"
#include <errno.h>
#include <elf.h>
#include <pmos/system.h>
#include <pmos/memory.h>
#include <pmos/load_data.h>
#include <elf.h>
#include "../elf/loader.h"
#include <inttypes.h>
#include <pmos/ipc.h>
#include "args.h"

struct module_descriptor_list {
    struct module_descriptor_list *next;
    uint64_t object_id;
    size_t size;
    char *cmdline;
    char *path;
    struct Service *service;
};

static void free_match_filters(match_filter_vector filters)
{
    struct MatchFilter f;
    VECTOR_FOREACH(filters, f) {
        free(f.key);
        if (f.strings) {
            char **s = f.strings;
            while (*s) {
                free(*s);
                ++s;
            }
            free(f.strings);
        }
    }
    VECTOR_FREE(filters);
}

void free_service(struct Service *service)
{
    if (!service)
        return;

    free(service->name);
    free(service->path);
    free(service->description);
    VECTOR_FREE(service->instances);

    struct Requirement r;
    VECTOR_FOREACH(service->requirements, r) {
        free(r.service_name);
    }
    VECTOR_FREE(service->requirements);

    struct Property p;
    VECTOR_FOREACH(service->properties, p) {
        release_property(&p);
    }
    VECTOR_FREE(service->properties);

    free_match_filters(service->match_filters);

    free(service);
}

struct Service *new_service()
{
    return calloc(1, sizeof(struct Service));
}

void *construct_pci_filter(struct PCIFilter f)
{
    pmos_bus_filter_conjunction *c = pmos_bus_filter_conjunction_create();
    if (!c)
        return NULL;

    pmos_bus_filter_equals *e = pmos_bus_filter_equals_create("device_type", "pci");
    if (!e) {
        print_str("Failed to create PCI device filter\n");
        goto error;
    }
    if (pmos_bus_filter_conjunction_add(c, e) != 0) {
        print_str("Failed to add value to the conjunction\n");
        pmos_bus_filter_free(e);
        goto error;
    }

    if (f.class) {
        e = pmos_bus_filter_equals_create("pci_class", f.class);
        if (!e) {
            print_str("Failed to create PCI device filter\n");
            goto error;
        }
        if (pmos_bus_filter_conjunction_add(c, e) != 0) {
            print_str("Failed to add value to the conjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }
    }
    if (f.prog_if) {
        e = pmos_bus_filter_equals_create("pci_interface", f.prog_if);
        if (!e) {
            print_str("Failed to create PCI device filter\n");
            goto error;
        }
        if (pmos_bus_filter_conjunction_add(c, e) != 0) {
            print_str("Failed to add value to the conjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }
    }
    if (f.subclass) {
        e = pmos_bus_filter_equals_create("pci_subclass", f.subclass);
        if (!e) {
            print_str("Failed to create PCI device filter\n");
            goto error;
        }
        if (pmos_bus_filter_conjunction_add(c, e) != 0) {
            print_str("Failed to add value to the conjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }
    }

return c;

error:
    pmos_bus_filter_free(c);
    return NULL;
}

void *construct_acpi_filter(char **strings)
{
    if (!strings || !strings[0])
        return NULL;

    pmos_bus_filter_disjunction *c = pmos_bus_filter_disjunction_create();
    if (!c)
        return NULL;

    char **s = strings;
    while (*s) {
        pmos_bus_filter_equals *e = NULL;

        e = pmos_bus_filter_equals_create("acpi_hid", *s);
        if (!e) {
            print_str("Failed to create ACPI HID filter\n");
            goto error;
        }
        if (pmos_bus_filter_disjunction_add(c, e) != 0) {
            print_str("Failed to add ACPI HID filter to disjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }

        e = pmos_bus_filter_equals_create("acpi_cid", *s);
        if (!e) {
            print_str("Failed to create ACPI CID filter\n");
            goto error;
        }

        if (pmos_bus_filter_disjunction_add(c, e) != 0) {
            print_str("Failed to add ACPI CID filter to disjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }

        ++s;
    }

    return c;
error:
    pmos_bus_filter_disjunction_free(c);
    return NULL;
}

void *construct_devices_filter(char **strings)
{
    if (!strings || !strings[0])
        return NULL;

    pmos_bus_filter_disjunction *c = pmos_bus_filter_disjunction_create();
    if (!c)
        return NULL;

    char **s = strings;
    while (*s) {
        pmos_bus_filter_equals *e = NULL;

        e = pmos_bus_filter_equals_create("device", *s);
        if (!e) {
            print_str("Failed to create ACPI HID filter\n");
            goto error;
        }

        if (pmos_bus_filter_disjunction_add(c, e) != 0) {
            print_str("Failed to add ACPI CID filter to disjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }

        ++s;
    }

    return c;
error:
    pmos_bus_filter_disjunction_free(c);
    return NULL;
}

void *construct_filter(struct Service *service)
{
    pmos_bus_filter_disjunction *d = pmos_bus_filter_disjunction_create();
    if (!d)
        return NULL;

    size_t entries = 0;
    const struct MatchFilter *f;
    VECTOR_FOREACH_PTR(service->match_filters, f) {
        if (!f->key) {
            print_str("Match filter with NULL key in service ");
            print_str(service->name);
            print_str("\n");
            continue;
        }
        if (!f->strings || !f->strings[0]) {
            print_str("Match filter with no strings in service ");
            print_str(service->name);
            print_str("\n");
            continue;
        }

        if (!strcmp(f->key, "pnp")) {
            void *a = construct_acpi_filter(f->strings);
            if (!a) {
                print_str("ACPI filter error (NULL) for service ");
                print_str(service->name);
                print_str("\n");
                continue;
            }

            if (pmos_bus_filter_disjunction_add(d, a) < 0) {
                print_str("Failed to add ACPI filter to disjunction\n");
                pmos_bus_filter_free(a);
                goto error;
            }
        } else if (!strcmp(f->key, "pci")) {
            void *a = construct_pci_filter(f->pci_filter);
            if (!a) {
                print_str("PCI filter error (NULL) for service ");
                print_str(service->name);
                print_str("\n");
                continue;
            }

            if (pmos_bus_filter_disjunction_add(d, a) < 0) {
                print_str("Failed to add ACPI filter to disjunction\n");
                pmos_bus_filter_free(a);
                goto error;
            }
        } else if (!strcmp(f->key, "devices")) {
            void *a = construct_devices_filter(f->strings);
            if (!a) {
                print_str("Devices filter error (NULL) for service ");
                print_str(service->name);
                print_str("\n");
                continue;
            }

            if (pmos_bus_filter_disjunction_add(d, a) < 0) {
                print_str("Failed to add ACPI filter to disjunction\n");
                pmos_bus_filter_free(a);
                goto error;
            }
        } else {
            print_str("Unknown filter key: ");
            print_str(f->key);
            print_str("\n");
            continue;
        }

        entries++;
    }

    if (entries == 0) {
        print_str("Service has no valid match filters\n");
        goto error;
    }

    return d;
error:
    pmos_bus_filter_free(d);
    return NULL;
}

uint64_t instance_id_counter = 1;

static int reserve_instances_vector(struct Service *service)
{
    int result = 0;
    size_t size = service->instances.size;
    VECTOR_RESERVE(service->instances, size + 1, result);
    if (result != 0) {
        print_str("Loader: Could not reserve space for service instance for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(result);
        print_str("\n");
        return -ENOMEM;
    }
    return 0;
}

int start_service(struct Service *service, uint64_t object_right, uint64_t optional_right_id)
{
    if (!service)
        return -EINVAL;

    print_str("Loader: starting service ");
    print_str(service->name ? service->name : "UNKNOWN NAME");
    print_str("\n");

    int result = reserve_instances_vector(service);
    if (result)
        return result;

    uint64_t group_id = {};
    syscall_r r       = syscall_new_process();
    if (r.result != SUCCESS) {
        print_str("Loader: Could not create process for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(r.result);
        print_str("\n");

        return r.result;
    }

    // Doesn't really matter if this fails
    syscall_set_task_name(r.value, service->name, strlen(service->name));

    syscall_r rr = create_task_group();
    if (rr.result != SUCCESS) {
        print_str("Loader: Could not create task group for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(rr.result);
        print_str("\n");

        syscall_kill_task(r.value);
        return rr.result;
    }

    group_id = rr.value;

    result_t rrr = add_task_to_group(r.value, group_id);
    if (rrr != SUCCESS) {
        print_str("Loader: Could not add task to group for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(r.result);
        print_str("\n");

        syscall_kill_task(r.value);
        remove_task_from_group(TASK_ID_SELF, group_id);
        return rrr;
    }

    right_request_t object_request = dup_right(object_right);
    if (object_request.result != SUCCESS) {
        print_str("Loader: Could not dup object right for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(object_request.result);
        print_str("\n");

        syscall_kill_task(r.value);
        remove_task_from_group(TASK_ID_SELF, group_id);
        return object_request.result;
    }

    auto object_result = transfer_right(group_id, object_request.right, 0);
    if (object_result.result) {
        print_str("Loader: Could not transfer object right to the task group for service ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(object_result.result);
        print_str("\n");
        
        delete_right(object_request.right);
        syscall_kill_task(r.value);
        remove_task_from_group(TASK_ID_SELF, group_id);
        return object_result.result;
    }

    pmos_right_t new_right = 0;
    if (optional_right_id) {
        auto result = transfer_right(group_id, optional_right_id, 0);
        if (result.result) {
            print_str("Loader: Could not transfer right to the task group for service ");
            print_str(service->name);
            print_str(". Error: ");
            print_hex(r.result);
            print_str("\n");

            syscall_kill_task(r.value);
            remove_task_from_group(TASK_ID_SELF, group_id);
            return result.result;
        }
        new_right = result.right;
    }

    uint64_t new_group_id = group_id;
    remove_task_from_group(TASK_ID_SELF, group_id);
    group_id = 0;

    // Task group tag
    struct AuxVecEntry *auxvec_entries[5];
    struct AuxVecEntry group_id_entry = {
        .entry_type = AT_TASK_GROUP_ID,
        .data_type = DATA_TYPE_EXTERNAL,
        .external_data = {
            .data = &new_group_id,
            .size = sizeof(new_group_id),
        },
    };
    struct AuxVecEntry mem_object_entry = {
        .entry_type = AT_MEM_OBJ_ID,
        .data_type = DATA_TYPE_EXTERNAL,
        .external_data = {
            .data = &object_result.right,
            .size = sizeof(object_result.right),
        },
    };

    auxvec_entries[0] = &group_id_entry;
    auxvec_entries[1] = &mem_object_entry;
    auxvec_entries[2] = NULL;

    const char *argc[5];
    argc[0] = service->name;
    argc[1] = NULL;

    char buff[64];
    if (new_right) {
        argc[1] = "--right-id";
        sprintf(buff, "%" PRIu64, new_right);
        argc[2] = buff;
        argc[3] = NULL;
    }

    result_t res = load_executable(r.value, object_right, 0, 0, 0, argc, NULL, (const struct AuxVecEntry **)auxvec_entries);
    //result_t res = syscall_load_executable(r.value, object_id, mem_region, 0);
    if (res != SUCCESS) {
        print_str("Loader: Could not load executable ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(res);
        print_str("\n");
        result = res;

        goto error;
    }

    service->state = STATE_STARTED;

    struct Instance instance = {
        .id = instance_id_counter++,
        .group_id = new_group_id,
        .parent = service,
        .state = STATE_STARTED,
    };

    VECTOR_PUSH_BACK(service->instances, instance);

    return 0;
error:
    if (group_id)
        remove_task_from_group(TASK_ID_SELF, group_id);

    if (r.value)
        syscall_kill_task(r.value);

    return result;
}

struct Context {
    int result;
    pmos_right_t transfered_right_ids[4];
    pmos_right_t *rights;
    uint64_t group_id;
};

static size_t start_callback(const char *name, size_t name_length, char *out_buffer, void *ctx)
{
    struct Context *context = ctx;
    size_t size = strlen("RIGHT0");

    if (context->result)
        return 0;

    if (name_length != size)
        return 0;

    int index = 0;
    if (!strncmp(name, "RIGHT0", size))
        index = 0;
    else if (!strncmp(name, "RIGHT1", size))
        index = 1;
    else if (!strncmp(name, "RIGHT2", size))
        index = 2;
    else if (!strncmp(name, "RIGHT3", size))
        index = 3;
    else
        return 0;

    if (!context->transfered_right_ids[index]) {
        if (!context->rights[index]) {
            context->result = -ENOENT;
            return 0;
        }

        auto result = transfer_right(context->group_id, context->rights[index], 0);
        if (result.result) {
            print_str("Loader: Could not transfer right to the task group while parsing cmdline. Error: ");
            print_hex(result.result);
            print_str("\n");

            context->result = result.result;
            return 0;
        }
        context->transfered_right_ids[index] = result.right;
        context->rights[index] = 0;
    }

    if (out_buffer)
        return sprintf(out_buffer, "%"PRIu64, context->transfered_right_ids[index]);
    else
        return snprintf(NULL, 0, "%"PRIu64, context->transfered_right_ids[index]);
}

int start_service_request(struct Service *service, const char *cmdline, size_t cmdline_length, pmos_right_t *extra_rights)
{
    if (!service)
        return -EINVAL;

    print_str("Loader: starting service ");
    print_str(service->name ? service->name : "UNKNOWN NAME");
    print_str("\n");

    if (!service->module)
        return -ENOENT;

    uint64_t object_right = service->module->object_id;

    int result = reserve_instances_vector(service);
    if (result)
        return result;

    uint64_t group_id = {};

    syscall_r r       = syscall_new_process();
    if (r.result != SUCCESS) {
        print_str("Loader: Could not create process for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(r.result);
        print_str("\n");

        return r.result;
    }

    // Doesn't really matter if this fails
    syscall_set_task_name(r.value, service->name, strlen(service->name));

    syscall_r rr = create_task_group();
    if (rr.result != SUCCESS) {
        print_str("Loader: Could not create task group for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(rr.result);
        print_str("\n");

        syscall_kill_task(r.value);
        return rr.result;
    }

    group_id = rr.value;

    result_t rrr = add_task_to_group(r.value, group_id);
    if (rrr != SUCCESS) {
        print_str("Loader: Could not add task to group for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(r.result);
        print_str("\n");

        syscall_kill_task(r.value);
        remove_task_from_group(TASK_ID_SELF, group_id);
        return rrr;
    }

    right_request_t object_request = dup_right(object_right);
    if (object_request.result != SUCCESS) {
        print_str("Loader: Could not dup object right for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(object_request.result);
        print_str("\n");

        syscall_kill_task(r.value);
        remove_task_from_group(TASK_ID_SELF, group_id);
        return object_request.result;
    }

    auto object_result = transfer_right(group_id, object_request.right, 0);
    if (object_result.result) {
        print_str("Loader: Could not transfer object right to the task group for service ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(object_result.result);
        print_str("\n");
        
        delete_right(object_request.right);
        syscall_kill_task(r.value);
        remove_task_from_group(TASK_ID_SELF, group_id);
        return object_result.result;
    }

    // Task group tag
    struct AuxVecEntry *auxvec_entries[4];
    struct AuxVecEntry group_id_entry = {
        .entry_type = AT_TASK_GROUP_ID,
        .data_type = DATA_TYPE_EXTERNAL,
        .external_data = {
            .data = &group_id,
            .size = sizeof(group_id),
        },
    };
    struct AuxVecEntry mem_object_entry = {
        .entry_type = AT_MEM_OBJ_ID,
        .data_type = DATA_TYPE_EXTERNAL,
        .external_data = {
            .data = &object_result.right,
            .size = sizeof(object_result.right),
        },
    };
    auxvec_entries[0] = &group_id_entry;
    auxvec_entries[1] = &mem_object_entry;
    auxvec_entries[2] = NULL;

    struct Args args = {};
    args_init(&args);

    if (!args_push_arg(&args, service->name)) {
        print_str("Failed to push arg\n");
        result = -ENOMEM;
        goto error;
    }

    struct Context ctx = {
        .rights = extra_rights,
        .group_id = group_id,
    };

    if (!parse_push_args(&args, cmdline, cmdline_length, start_callback, &ctx)) {
        print_str("Failed to parse cmdline\n");
        result = -ENOMEM;
        goto error;
    }
    if (ctx.result) {
        result = ctx.result;
        goto error;
    }

    uint64_t new_group_id = group_id;
    remove_task_from_group(TASK_ID_SELF, group_id);
    group_id = 0;


    result_t res = load_executable(r.value, object_right, 0, 0, 0, args_get_argv(&args), NULL, (const struct AuxVecEntry **)auxvec_entries);
    //result_t res = syscall_load_executable(r.value, object_id, mem_region, 0);
    if (res != SUCCESS) {
        print_str("Loader: Could not load executable ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(res);
        print_str("\n");
        result = res;

        goto error;
    }

    service->state = STATE_STARTED;

    struct Instance instance = {
        .id = instance_id_counter++,
        .group_id = new_group_id,
        .parent = service,
        .state = STATE_STARTED,
    };

    VECTOR_PUSH_BACK(service->instances, instance);

    args_release(&args);

    return 0;
error:
    args_release(&args);

    if (group_id)
        remove_task_from_group(TASK_ID_SELF, group_id);

    if (r.value)
        syscall_kill_task(r.value);

    return result;
}

struct Service *services = NULL;

void push_services(struct Service *s)
{
    while (s) {
        struct Service *ss = s->next;
        s->next = services;
        services = s;

        print_str("Added service ");
        if (s->name)
            print_str(s->name);
        print_str("\n");

        s = ss;
    }
}

struct module_descriptor_list *find_module(char *path);

void match_services()
{
    for (struct Service *s = services; s; s = s->next) {
        if (s->module)
            continue;

        const char *name;
        if (s->name)
            name = s->name;
        else
            name = "<unknown>";

        if (!s->path) {
            print_str("Warning: module ");
            print_str(name);
            print_str(" with empty path!\n");
            continue;
        }

        struct module_descriptor_list *module = find_module(s->path);
        if (!module) {
            print_str("Module not found for service ");
            print_str(name);
            print_str(", path: ");
            print_str(s->path);
            print_str("\n");
            continue;
        }

        if (module->service) {
            print_str("Warning: module ");
            print_str(module->path);
            print_str(" already has a service!\n");
            continue;
        }

        module->service = s;
        s->module = module;
    }
}

const char *run_type_str(struct Service *service)
{
    switch (service->run_type) {
    case RUN_MANUAL:
        return "manual";
    case RUN_ALWAYS_ONCE:
        return "always_once";
    case RUN_FIRST_MATCH_ONCE:
        return "fire_match_once";
    case RUN_FOR_EACH_MATCH:
        return "for_each_match";
    case RUN_UNKNOWN:
    default:
        return "unknown";
    }
}

static bool service_set_property(pmos_bus_object_t *object, struct Property *property)
{
    switch (property->type) {
    case PROPERTY_LIST:
        return pmos_bus_object_set_property_list(object, property->name, (const char **)property->list);
    case PROPERTY_STRING:
        return pmos_bus_object_set_property_string(object, property->name, property->string);
    case PROPERTY_INTEGER:
        return pmos_bus_object_set_property_integer(object, property->name, property->integer);
    }
}

pmos_bus_object_t *construct_pmbus_object(struct Service *service)
{
    const char *name = service->name;
    if (!name)
        name = "UNKNOWN";

    const char *run_type = run_type_str(service);
    
    ssize_t name_size = snprintf(NULL, 0, "bootstrapd_service_%s", name);
    char name_buf[name_size + 1];
    sprintf(name_buf, "bootstrapd_service_%s", name);

    pmos_bus_object_t *object = pmos_bus_object_create();
    if (!object) {
        print_str("Loader: Failed to create pmbus object...\n");
        goto error;
    }

    if (!pmos_bus_object_set_name(object, name_buf)) {
        print_str("Failed to set object name\n");
        goto error;
    }

    if (!pmos_bus_object_set_property_string(object, "type", "service")) {
        print_str("Failed to set object type\n");
        goto error;
    }

    if (!pmos_bus_object_set_property_string(object, "run_type", run_type)) {
        print_str("Failed to set service run type\n");
        goto error;
    }

    struct Property *p;
    VECTOR_FOREACH_PTR(service->properties, p) {
        if (!service_set_property(object, p)) {
            print_str("Failed to set service property\n");
            goto error;
        }
    }

    return object;
error:
    pmos_bus_object_free(object);
    return NULL;
}

void release_property(struct Property *property)
{
    switch (property->type) {
    case PROPERTY_INTEGER:
        // Do nothing
        break;
    case PROPERTY_STRING:
        free(property->string);
        break;
    case PROPERTY_LIST: {
        char **ptr = property->list;
        while (ptr && *ptr) {
            free(*ptr);
            ++ptr;
        }
        free(property->list);
    }
        break;
    }

    property->type = PROPERTY_INTEGER;
    property->integer = 0;
}

void handle_start_service(struct Service *service, IPC_Start_Service *msg, size_t msg_len, pmos_right_t *optional_reply_right, pmos_right_t *extra_rights)
{
    char *cmdline = msg->cmdline;
    size_t cmdline_length = msg_len - sizeof(*msg);
    uint64_t id = 0;

    int result = start_service_request(service, cmdline, cmdline_length, extra_rights);
    if (!result) {
        id = VECTOR_BACK(service->instances).id;
    } 

    if (*optional_reply_right) {
        IPC_Start_Service_Result reply = {
            .type = IPC_Start_Service_Result_NUM,
            .result = result,
            .instance_id = id,
        };

        auto send_result = send_message_right(*optional_reply_right, 0, &reply, sizeof(reply), NULL, SEND_MESSAGE_DELETE_RIGHT);
        if (!send_result.result) {
            *optional_reply_right = 0;
        } else {
            print_str("Loader: Failed to reply to service start request, error ");
            print_hex(send_result.result);
            print_str("\n");
        }
    } else {
        if (result) {
            print_str("Loader: Failed to start service ");
            print_str(service->name);
            print_str(", error: ");
            print_hex(-result);
            print_str("\n");
        }
    }
}

static int service_right_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void * ctx, struct pmos_msgloop_data *data)
{
    (void)data;
    struct Service *service = ctx;

    if (desc->size < IPC_MIN_SIZE) {
        print_str("Loader: Recieved a message for a service that is too small...\n");
        return 0;
    }

    switch (IPC_TYPE(buff)) {
    case IPC_Start_Service_NUM:
        if (desc->size < IPC_RIGHT_SIZE(IPC_Start_Service_NUM)) {
            print_str("Loader: recieved IPC_Start_Service that is too small\n");
            break;
        }

        IPC_Start_Service *i = buff;

        handle_start_service(service, i, desc->size, reply_right, extra_rights);

        break;
    default:
        print_str("Loader: recieved unknown message for service, with type ");
        print_hex(IPC_TYPE(buff));
        print_str("\n");
        break;
    }

    return 0;
}

extern uint64_t loader_port;
extern struct pmos_msgloop_data msgloop_data;

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

    pmos_msgloop_node_set(&service->service_right_node, recieve_right, service_right_callback, service);
    pmos_msgloop_insert(&msgloop_data, &service->service_right_node);

    return true;
}