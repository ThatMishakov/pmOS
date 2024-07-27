#include <uacpi/utilities.h>
#include <uacpi/resources.h>

#define PC_COM_PNP_ID "PNP0500"
#define NS16550A_PNP_ID "PNP0501"

static uacpi_ns_iteration_decision match_pc_com(void *user, uacpi_namespace_node *node)
{
    printf("Found COM!\n");
    return UACPI_NS_ITERATION_DECISION_CONTINUE;
}

static uacpi_ns_iteration_decision match_ns16550a(void *user, uacpi_namespace_node *node)
{
    printf("Found NS16550A!\n");
    return UACPI_NS_ITERATION_DECISION_CONTINUE;
}

void find_com()
{
    uacpi_find_devices(PC_COM_PNP_ID, match_pc_com, NULL);
    uacpi_find_devices(NS16550A_PNP_ID, match_ns16550a, NULL);
}