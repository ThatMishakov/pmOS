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
#include <kernel/block.h>
#include <main.h>
#include <phys_map/phys_map.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <uacpi/event.h>
#include <uacpi/utilities.h>
#include <uacpi/sleep.h>
#include <pthread.h>
#include <pmos/special.h>
#include <uacpi/osi.h>
#include <uacpi/notify.h>
#include <uacpi/context.h>


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
        printf("RSDT table found at 0x%llx\n", rsdp_desc);
    }
    free(message);
}

void acpi_pci_init();
void init_pci();
void find_acpi_devices();
void gpio_initialize();
int power_button_init();

#include <uacpi/uacpi.h>
#include <uacpi/event.h>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rdsp_address)
{
    if (!have_rsdp)
        return UACPI_STATUS_NOT_FOUND;

    *out_rdsp_address = rsdp_desc;
    return UACPI_STATUS_OK;
}

void init_ec();
void ec_finalize();

int acpi_init() {
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uacpi_initialize error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    //uacpi_context_set_log_level(UACPI_LOG_DEBUG);

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

    return 0;
}

int system_shutdown(void) {
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
    //disable_interrupts();

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

void *shutdown_thread(void *) {
    pmos_request_io_permission();
    printf("Shutting down in 3 seconds...\n");
    set_affinity(TASK_ID_SELF, -1, 0);
    //uint64_t start = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP).value;
    sleep(3);
    //uint64_t end = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP).value;
    //printf("Time difference: %llu\n", end - start);
    system_shutdown();

    return NULL;
}

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    (void)ctx;
    printf("Power button pressed\n");
    pthread_t thread;
    pthread_create(&thread, NULL, shutdown_thread, NULL);
    pthread_detach(thread);
    return UACPI_INTERRUPT_HANDLED;
}

static uacpi_status handle_power_button_event(uacpi_handle ctx, uacpi_namespace_node* node, uacpi_u64 value) {
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

static uacpi_iteration_decision power_button_uacpi_stuff(void* ctx, uacpi_namespace_node *node, uint32_t depth) {
    (void)depth;
    (void)ctx;

    uacpi_install_notify_handler(node, handle_power_button_event, NULL);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

#define ACPI_HID_POWER_BUTTON "PNP0C0C"

int power_button_init(void) {
    uacpi_status ret = uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
	    handle_power_button, UACPI_NULL
    );
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "failed to install power button event callback: %s\n", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    uacpi_find_devices(ACPI_HID_POWER_BUTTON, power_button_uacpi_stuff, NULL);

    return 0;
}

void find_com();
void init_ioapic();
void init_cpus();

void find_acpi_devices()
{
    find_com();
}

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
    init_pci();

    int i = acpi_init();
    if (i != 0) {
        printf("Warning: Could not initialize uACPI\n");
        return;
    }

    //init_pci();

    printf("Walked ACPI tables! ACPI revision: %i\n", acpi_revision);

}