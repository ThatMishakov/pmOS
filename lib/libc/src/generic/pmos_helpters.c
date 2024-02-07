#include <pmos/helpers.h>
#include <stdlib.h>

result_t get_message(Message_Descriptor* desc, unsigned char** message, pmos_port_t port)
{
    result_t result = syscall_get_message_info(desc, port, 0);
    if (result != SUCCESS)
        return result;

    *message = malloc(desc->size);
    if (*message == NULL) {
        return 1; // This needs to be changed
    }

    result = get_first_message(*message, 0, port);
    if (result != SUCCESS) {
        free(*message);
    }

    return result;
}