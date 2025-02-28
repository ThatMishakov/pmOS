/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <acpi/acpi.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <kernel/block.h>
#include <main.h>
#include <phys_map/phys_map.h>
#include <pmos/hashmap.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/pmbus_object.h>
#include <pmos/ports.h>
#include <pmos/special.h>
#include <pmos/system.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uacpi/context.h>
#include <uacpi/event.h>
#include <uacpi/notify.h>
#include <uacpi/osi.h>
#include <uacpi/sleep.h>
#include <uacpi/utilities.h>
#include <vector.h>

void init_acpi();

bool have_rsdp = false;
extern uint64_t rsdp_desc;

extern int acpi_revision;

// Returns -1 on error or ACPI version otherwise
int walk_acpi_tables();
int check_table(ACPISDTHeader *header);

int acpi_revision = -1;

uint64_t rsdp_desc = 0;

typedef struct acpi_mem_map_node {
    uint64_t phys;
    size_t size_bytes;
    void *virt;
    struct acpi_mem_map_node *next;
} acpi_mem_map_node;

acpi_mem_map_node acpi_mem_map_list_dummy = {0, 0, 0, NULL};

void acpi_mem_map_list_push_front(acpi_mem_map_node *n)
{
    n->next                      = acpi_mem_map_list_dummy.next;
    acpi_mem_map_list_dummy.next = n;
}

void *acpi_map_and_get_virt(uint64_t phys, size_t size)
{
    for (acpi_mem_map_node *p = acpi_mem_map_list_dummy.next; p != NULL; p = p->next) {

        if (p->phys <= phys && (p->size_bytes >= size + (((size_t)phys - (size_t)p->phys))))
            return (void *)((uint64_t)phys - (uint64_t)p->phys + (uint64_t)p->virt);
    }

    void *virt = map_phys(phys, size);
    if (virt == NULL)
        return NULL;
    // if (virt == NULL) panic("Panic: Could not map meory\n");

    acpi_mem_map_node *n = malloc(sizeof(acpi_mem_map_node));
    if (n == NULL) {
        unmap_phys(virt, size);
        return NULL;
    }
    // if (n == NULL) panic("Panic: Could not allocate memory\n");

    n->phys       = phys;
    n->size_bytes = size;
    n->virt       = virt;
    n->next       = NULL;
    acpi_mem_map_list_push_front(n);
    return virt;
}

void acpi_map_release_exact(void *virt, size_t size)
{
    acpi_mem_map_node *p = &acpi_mem_map_list_dummy;
    for (; p->next != NULL; p = p->next) {
        if (p->next->virt == virt && p->next->size_bytes == size)
            break;
    }

    acpi_mem_map_node *temp = p->next->next;
    unmap_phys(p->next->virt, p->next->size_bytes);
    free(p->next);
    p->next = temp;
}

RSDT *rsdt_phys = NULL;

XSDT *xsdt_phys = NULL;

static const char *loader_port_name = "/pmos/loader";

void request_acpi_tables()
{
    IPC_ACPI_Request_RSDT request = {IPC_ACPI_Request_RSDT_NUM, 0, configuration_port};

    ports_request_t loader_port = get_port_by_name(loader_port_name, strlen(loader_port_name), 0);
    if (loader_port.result != SUCCESS) {
        printf("Warning: Could not get loader port. Error %li\n", loader_port.result);
        return;
    }

    result_t result = send_message_port(loader_port.port, sizeof(request), (char *)&request);
    if (result != SUCCESS) {
        printf("Warning: Could not send message to get the RSDT\n");
        return;
    }

    Message_Descriptor desc = {};
    unsigned char *message  = NULL;
    result                  = get_message(&desc, &message, configuration_port);

    if (result != SUCCESS) {
        printf("Warning: Could not get message\n");
        return;
    }

    if (desc.size < sizeof(IPC_ACPI_RSDT_Reply)) {
        printf("Warning: Recieved message which is too small\n");
        free(message);
        return;
    }

    IPC_ACPI_RSDT_Reply *reply = (IPC_ACPI_RSDT_Reply *)message;

    if (reply->type != IPC_ACPI_RSDT_Reply_NUM) {
        printf("Warning: Recieved unexepcted message type\n");
        free(message);
        return;
    }

    if (reply->result != 0) {
        printf("Warning: Did not get RSDT table: %i\n", reply->result);
    } else {
        rsdp_desc = reply->descriptor;
        have_rsdp = true;
        printf("RSDT table found at 0x%" PRIu64 "\n", rsdp_desc);
    }
    free(message);
}

void acpi_pci_init();
void init_pci();
void find_acpi_devices();
void gpio_initialize();
int power_button_init();

#include <uacpi/event.h>
#include <uacpi/uacpi.h>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rdsp_address)
{
    if (!have_rsdp)
        return UACPI_STATUS_NOT_FOUND;

    *out_rdsp_address = rsdp_desc;
    return UACPI_STATUS_OK;
}

void init_ec();
void ec_finalize();
void publish_devices();

int acpi_init()
{
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uacpi_initialize error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    // uacpi_context_set_log_level(UACPI_LOG_DEBUG);

    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uacpi_namespace_load error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

#if defined(__x86_64__) || defined(__i386__)
    ret = uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uacpi_set_interrupt_model error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }
#endif

    acpi_pci_init();
    init_ec();

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uacpi_namespace_initialize error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uACPI GPE initialization error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    ec_finalize();
    gpio_initialize();
    find_acpi_devices();
    power_button_init();
    publish_devices();

    return 0;
}

int system_shutdown(void)
{
    /*
     * Prepare the system for shutdown.
     * This will run the \_PTS & \_SST methods, if they exist, as well as
     * some work to fetch the \_S5 and \_S0 values to make system wake
     * possible later on.
     */
    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "failed to prepare for sleep: %s", uacpi_status_to_string(ret));
        return -EIO;
    }

    /*
     * This is where we disable interrupts to prevent anything from
     * racing with our shutdown sequence below.
     */
    // disable_interrupts();

    /*
     * Actually do the work of entering the sleep state by writing to the hardware
     * registers with the values we fetched during preparation.
     * This will also disable runtime events and enable only those that are
     * needed for wake.
     */
    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "failed to enter sleep: %s", uacpi_status_to_string(ret));
        return -EIO;
    }

    /*
     * Technically unreachable code, but leave it here to prevent the compiler
     * from complaining.
     */
    return 0;
}

syscall_r __pmos_syscall_set_attr(uint64_t pid, uint32_t attr, unsigned long value);
syscall_r syscall_prepare_sleep(uint64_t pid, uint32_t attr, unsigned long value);

struct MemoryRegion {
    uint64_t phys_start;
    uint64_t length;
    uint64_t type;
};

struct NvsRegion {
    uint64_t phys_start;
    uint64_t size_bytes;
    void *virt_start;
    void *save_to;
};

void nvs_region_free(struct NvsRegion *region)
{
    assert(region);
    free(region->save_to);
    unmap_phys(region->virt_start, region->size_bytes);
}

VECTOR_TYPEDEF(struct NvsRegion, NVSRegionVector);
NVSRegionVector nvs_vector = VECTOR_INIT;

int init_sleep()
{
    static int status = 0;
    if (status < 0) {
        return -1;
    } else if (status == 0) {
        syscall_r wakeup_vec = __pmos_syscall_set_attr(0, 4, 0);
        if (wakeup_vec.result) {
            fprintf(stderr, "[devicesd] Error: Did not get wakeup vector from kernel...\n");
            status = -1;
            return -1;
        }

        uacpi_status result = uacpi_set_waking_vector(wakeup_vec.value, 0);
        if (result != UACPI_STATUS_OK) {
            fprintf(stderr, "[devicesd] Error: could not set uacpi_set_waking_vector\n");
            status = -1;
            return -1;
        }

        syscall_r nvs_cnt = __pmos_syscall_set_attr(0, 5, 0);
        if (nvs_cnt.result) {
            fprintf(stderr, "Could not get the number of ACPI NVS entries\n");
            status = -1;
            return -1;
        }

        size_t count = nvs_cnt.value;

        int resize_result = 0;
        VECTOR_RESERVE(nvs_vector, count, resize_result);
        if (resize_result) {
            fprintf(stderr, "Failed to reserve memory for NVS regions vector\n");
            status = -1;
            return -1;
        }

        struct MemoryRegion regions[count];
        nvs_cnt = __pmos_syscall_set_attr(0, 6, (unsigned long)&regions[0]);
        if (nvs_cnt.result) {
            fprintf(stderr, "Failed to get the ACPI NVS regions: %i\n", nvs_cnt.result);
            status = -1;
            return -1;
        }

        for (size_t i = 0; i < count; ++i) {
            void *mapping = map_phys(regions[i].phys_start, regions[i].length);
            if (!mapping) {
                struct NvsRegion r;
                VECTOR_FOREACH(nvs_vector, r)
                    nvs_region_free(&r);

                VECTOR_FREE(nvs_vector);
                status = -1;
                return -1;
            }

            void *memory = malloc(regions[i].length);
            if (!memory) {
                unmap_phys(mapping, regions[i].length);

                struct NvsRegion r;
                VECTOR_FOREACH(nvs_vector, r)
                    nvs_region_free(&r);

                VECTOR_FREE(nvs_vector);
                status = -1;
                return -1;
            }

            struct NvsRegion r = {
                .phys_start = regions[i].phys_start,
                .size_bytes = regions[i].length,
                .save_to = memory,
                .virt_start = mapping,
            };
            VECTOR_PUSH_BACK(nvs_vector, r);
        }

        return 0;
    } else {
        return 0;
    }
}

void save_nvs()
{
    struct NvsRegion r;
    VECTOR_FOREACH(nvs_vector, r)
    memcpy(r.save_to, r.virt_start, r.size_bytes);
}
void restore_nvs()
{
    struct NvsRegion r;
    VECTOR_FOREACH(nvs_vector, r)
    memcpy(r.virt_start, r.save_to, r.size_bytes);
}

void call_sleep_handlers() {}

void call_sleep_handlers_wakeup() {}

void restore_ioapics();

int system_sleep()
{
    int status = init_sleep();
    if (status < 0) {
        fprintf(stdout, "no sleep\n");
        return status;
    }

    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S3);
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "failed to prepare for sleep: %s", uacpi_status_to_string(ret));
        return -EIO;
    }

    volatile bool entered_sleep = false;
    int result                  = syscall_prepare_sleep(0, 3, 0).result;
    assert(!result);
    if (entered_sleep) {
        restore_ioapics();
        restore_nvs();
        uacpi_prepare_for_wake_from_sleep_state(UACPI_SLEEP_STATE_S3);
        int result = uacpi_wake_from_sleep_state(UACPI_SLEEP_STATE_S3);
        call_sleep_handlers_wakeup();
        printf("uacpi_wake_from_sleep_state: %i\n", result);
    } else {
        entered_sleep = true;
        call_sleep_handlers();
        save_nvs();
        ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S3);
        assert(!uacpi_unlikely_error(ret));
    }

    return 0;
}

pthread_mutex_t power_lock = PTHREAD_MUTEX_INITIALIZER;
void system_sleep_wrapper()
{
    pthread_mutex_lock(&power_lock);
    system_sleep();
    pthread_mutex_unlock(&power_lock);
}

void *shutdown_thread(void *)
{
    pmos_request_io_permission();
    printf("Shutting down in 3 seconds...\n");
    set_affinity(TASK_ID_SELF, -1, 0);
    // uint64_t start = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP).value;
    // sleep(3);
    // uint64_t end = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP).value;
    // printf("Time difference: %llu\n", end - start);
    system_sleep();

    return NULL;
}

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx)
{
    (void)ctx;
    printf("Power button pressed\n");
    pthread_t thread;
    pthread_create(&thread, NULL, shutdown_thread, NULL);
    pthread_detach(thread);
    return UACPI_INTERRUPT_HANDLED;
}

static uacpi_status handle_power_button_event(uacpi_handle ctx, uacpi_namespace_node *node,
                                              uacpi_u64 value)
{
    (void)ctx;
    (void)node;

    if (value != 0x80) {
        printf("Unknown power button event: %llu\n", value);
        return UACPI_STATUS_OK;
    }

    printf("Power button event\n");

    handle_power_button(NULL);

    return UACPI_STATUS_OK;
}

static uacpi_iteration_decision power_button_uacpi_stuff(void *ctx, uacpi_namespace_node *node,
                                                         uint32_t depth)
{
    (void)depth;
    (void)ctx;

    uacpi_install_notify_handler(node, handle_power_button_event, NULL);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

#define ACPI_HID_POWER_BUTTON "PNP0C0C"

int power_button_init(void)
{
    uacpi_status ret = uacpi_install_fixed_event_handler(UACPI_FIXED_EVENT_POWER_BUTTON,
                                                         handle_power_button, UACPI_NULL);
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "failed to install power button event callback: %s\n",
                uacpi_status_to_string(ret));
        return -ENODEV;
    }

    uacpi_find_devices(ACPI_HID_POWER_BUTTON, power_button_uacpi_stuff, NULL);

    return 0;
}

void find_com();
void init_ioapic();
void init_cpus();

void find_acpi_devices() { find_com(); }

void init_acpi()
{
    printf("Info: Initializing ACPI...\n");

    request_acpi_tables();

    acpi_revision = walk_acpi_tables();

    if (acpi_revision == -1) {
        printf("Warning: Did not initialize ACPI\n");
    } else {
        printf("Info: ACPI revision: %i\n", acpi_revision);
    }

    init_cpus();
#if defined(__x86_64__) || defined(__i386__)
    init_ioapic();
#endif

    int i = acpi_init();
    if (i != 0) {
        printf("Warning: Could not initialize uACPI\n");
        return;
    }

    // init_pci();

    printf("Walked ACPI tables! ACPI revision: %i\n", acpi_revision);
}
extern pmos_port_t main_port;

pmos_hashtable_t register_requests = PMOS_HASHTABLE_INITIALIZER;
struct RegisterRequest {
    pmos_hashtable_ll_t hash_head;
    uint64_t id;
    char *acpi_name;
    uint8_t *message_data;
    size_t message_data_size;
    uint64_t pmbus_sequence_id;
};
uint64_t next_counter = 1;

static size_t register_request_hash(pmos_hashtable_ll_t *element, size_t total_size)
{
    struct RegisterRequest *r = container_of(element, struct RegisterRequest, hash_head);
    return (r->id * 7) % total_size;
}

static bool hash_equals(pmos_hashtable_ll_t *element, void *value)
{
    struct RegisterRequest *rr = container_of(element, struct RegisterRequest, hash_head);
    return rr->id == *(uint64_t *)value;
}

static size_t hash_for_value(void *value, size_t total_size)
{
    return (*(uint64_t *)(value) * 7) % total_size;
}

static pmos_port_t pmbus_port    = 0;
static bool pmbus_port_requested = false;
const char *pmbus_port_name      = "/pmos/pmbus";
int request_pmbus_port(pmos_port_t *port_out)
{
    assert(port_out);
    if (pmbus_port) {
        *port_out = pmbus_port;
        return 0;
    }

    *port_out = 0;
    if (pmbus_port_requested)
        return 0;

    int result = (int)request_named_port(pmbus_port_name, strlen(pmbus_port_name), main_port, 0);
    if (result < 0) {
        fprintf(stderr, "devicesd: Fauled to request pmbus port: %i (%s)\n", result,
                strerror(-result));
        return -1;
    }

    pmbus_port_requested = true;
    return 0;
}

static void send_foreach(pmos_hashtable_ll_t *element, void *ctx)
{
    (void)ctx;

    struct RegisterRequest *r = container_of(element, struct RegisterRequest, hash_head);
    if (!r->message_data)
        return;

    int result = (int)send_message_port(pmbus_port, r->message_data_size, r->message_data);
    if (result < 0) {
        fprintf(stderr, "devicesd: Couldn't send message to pmbus: %i (%s)\n", result,
                strerror(-result));
    }

    free(r->message_data);
    r->message_data      = NULL;
    r->message_data_size = 0;
}

static void publish_send() { hashtable_foreach(&register_requests, send_foreach, NULL); }

int send_register_object(struct RegisterRequest *r)
{
    pmos_port_t pmbus_port;
    int result = request_pmbus_port(&pmbus_port);
    if (result < 0) {
        fprintf(stderr, "Failed to request pmbus port\n");
        return -1;
    }

    result = hashmap_add(&register_requests, register_request_hash, &r->hash_head);
    if (result < 0) {
        fprintf(stderr, "devicesd: Failed to add request to hash map\n");
        return result;
    }

    if (pmbus_port == 0)
        // This will be contineud once the port is obtained
        return 0;

    result = (int)send_message_port(pmbus_port, r->message_data_size, r->message_data);
    if (result < 0) {
        fprintf(stderr, "devicesd: Couldn't send message to pmbus: %i (%s)\n", result,
                strerror(-result));
        hashtable_delete(&register_requests, register_request_hash, &r->hash_head);
        return -1;
    }
    free(r->message_data);
    r->message_data      = NULL;
    r->message_data_size = 0;

    return 0;
}

void publish_object_reply(Message_Descriptor *desc, IPC_BUS_Publish_Object_Reply *r)
{
    (void)desc;
    uint64_t idx = r->user_arg;

    pmos_hashtable_ll_t *e = hashtable_find(&register_requests, &idx, hash_for_value, hash_equals);
    if (!e) {
        fprintf(stderr,
                "[devicesd] Warning: Recieved IPC_BUS_Publish_Object_Reply arg %" PRIi64
                " , but didn't find value\n",
                idx);
        return;
    }

    struct RegisterRequest *rr = container_of(e, struct RegisterRequest, hash_head);
    if (r->result == 0) {
        printf("[devicesd] Published %s, sequence number %" PRIi64 "!\n", rr->acpi_name,
               r->sequence_number);
        rr->pmbus_sequence_id = r->sequence_number;
    } else {
        printf("[devicesd] Failed to publish %s! Result %i\n", rr->acpi_name, r->result);
    }
}

void named_port_notification(Message_Descriptor *desc, IPC_Kernel_Named_Port_Notification *n)
{
    if (desc->sender != 0) {
        fprintf(stderr,
                "[devicesd] Warning: recieved IPC_Kernel_Named_Port_Notification from task %" PRIi64
                " , expected kernel (0)\n",
                desc->sender);
        return;
    }

    size_t len = NAMED_PORT_NOTIFICATION_STR_LEN(desc->size);
    if (len == strlen(pmbus_port_name) && !memcmp(pmbus_port_name, n->port_name, len)) {
        pmbus_port = n->port_num;

        publish_send();
    }
}

int register_object(pmos_bus_object_t *object_owning)
{
    int result                 = -1;
    struct RegisterRequest *rr = calloc(sizeof(*rr), 1);
    if (!rr) {
        fprintf(stderr, "devicesd: Couldn't allocate memory for RegisterRequest\n");
        goto end;
    }

    rr->acpi_name = strdup(pmos_bus_object_get_name(object_owning));
    if (!rr->acpi_name) {
        fprintf(stderr, "devicesd: Couldn't allocate memory for acpi_name\n");
        goto end;
    }

    rr->id = next_counter++;

    uint8_t *msg    = NULL;
    size_t msg_size = 0;

    if (!pmos_bus_object_serialize_ipc(object_owning, main_port, rr->id, main_port,
                                       pmos_process_task_group(), &msg, &msg_size)) {
        fprintf(stderr, "Couldn't serialize pmbus object\n");
        goto end;
    }

    rr->message_data      = msg;
    rr->message_data_size = msg_size;

    result = send_register_object(rr);

end:
    if (result && rr) {
        free(rr->message_data);
        free(rr->acpi_name);
        free(rr);
    }

    pmos_bus_object_free(object_owning);
    return result;
}

static uacpi_iteration_decision acpi_init_one_device(void *ctx, uacpi_namespace_node *node,
                                                     uacpi_u32 node_depth)
{
    uacpi_namespace_node_info *info = NULL;
    (void)node_depth;
    (void)ctx;
    const char *path              = NULL;
    pmos_bus_object_t *bus_object = NULL;

    uacpi_status ret = uacpi_get_namespace_node_info(node, &info);
    if (uacpi_unlikely_error(ret)) {
        path = uacpi_namespace_node_generate_absolute_path(node);
        fprintf(stderr, "unable to retrieve node %s information: %s\n", path,
                uacpi_status_to_string(ret));
        goto _continue;
    }

    if (info->flags & (UACPI_NS_NODE_INFO_HAS_HID | UACPI_NS_NODE_INFO_HAS_CID)) {
        path = uacpi_namespace_node_generate_absolute_path(node);
        if (!path) {
            fprintf(stderr, "Unable to get path for uACPI node...\n");
            goto _continue;
        }

        bus_object = pmos_bus_object_create();
        if (!bus_object) {
            fprintf(stderr, "unable to create bus_object...\n");
            goto _continue;
        }

        if (!pmos_bus_object_set_name(bus_object, path)) {
            fprintf(stderr, "Failed to set object name\n");
            goto _continue;
        }

        if ((info->flags & UACPI_NS_NODE_INFO_HAS_ADR) &&
            !pmos_bus_object_set_property_integer(bus_object, "acpi_adr", info->adr)) {
            fprintf(stderr, "Failed to set object acpi_adr\n");
            goto _continue;
        }

        if ((info->flags & UACPI_NS_NODE_INFO_HAS_HID) &&
            !pmos_bus_object_set_property_string(bus_object, "acpi_hid", info->hid.value)) {
            fprintf(stderr, "Failed to set object acpi_hid\n");
            goto _continue;
        }

        if ((info->flags & UACPI_NS_NODE_INFO_HAS_UID) &&
            !pmos_bus_object_set_property_string(bus_object, "acpi_uid", info->uid.value)) {
            fprintf(stderr, "Failed to set object acpi_hid\n");
            goto _continue;
        }

        if ((info->flags & UACPI_NS_NODE_INFO_HAS_CLS) &&
            !pmos_bus_object_set_property_string(bus_object, "acpi_cls", info->cls.value)) {
            fprintf(stderr, "Failed to set object acpi_hid\n");
            goto _continue;
        }

        if (info->flags & UACPI_NS_NODE_INFO_HAS_CID) {
            uint32_t num_ids = info->cid.num_ids;
            const char *strings[num_ids + 1];
            for (uint32_t i = 0; i < num_ids; ++i) {
                strings[i] = info->cid.ids[i].value;
            }
            strings[num_ids] = NULL;

            if (!pmos_bus_object_set_property_list(bus_object, "acpi_cid", strings)) {
                fprintf(stderr, "Failed to set object acpi_cid\n");
                goto _continue;
            }
        }

        if (info->flags & UACPI_NS_NODE_INFO_HAS_SXD) {
            uint64_t value = (info->sxd[0]) || ((uint64_t)info->sxd[1] << 8) ||
                             ((uint64_t)info->sxd[2] << 16) || ((uint64_t)info->sxd[3] << 24);
            if (!pmos_bus_object_set_property_integer(bus_object, "acpi_sxd", value)) {
                fprintf(stderr, "Failed to set object acpi_sxd\n");
                goto _continue;
            }
        }

        if (info->flags & UACPI_NS_NODE_INFO_HAS_SXW) {
            uint64_t value = (info->sxw[0]) || ((uint64_t)info->sxw[1] << 8) ||
                             ((uint64_t)info->sxw[2] << 16) || ((uint64_t)info->sxw[3] << 24) ||
                             ((uint64_t)info->sxw[4] << 32);
            if (!pmos_bus_object_set_property_integer(bus_object, "acpi_sxw", value)) {
                fprintf(stderr, "Failed to set object acpi_sxw\n");
                goto _continue;
            }
        }

        int result = register_object(bus_object);
        bus_object = NULL;
        if (result < 0) {
            fprintf(stderr, "Failed to register object...\n");
            goto _continue;
        }
    }
_continue:
    pmos_bus_object_free(bus_object);
    uacpi_free_namespace_node_info(info);
    uacpi_free_absolute_path(path);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void publish_devices()
{
    uacpi_namespace_for_each_child(uacpi_namespace_root(), acpi_init_one_device, UACPI_NULL,
                                   UACPI_OBJECT_DEVICE_BIT, UACPI_MAX_DEPTH_ANY, UACPI_NULL);
}