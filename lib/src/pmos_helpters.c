#include <pmos/helpers.h>
#include <stdlib.h>

result_t get_message(Message_Descriptor* desc, unsigned char** message)
{
    result_t result = syscall_get_message_info(desc);
    if (result != SUCCESS)
        return result;

    *message = malloc(desc->size);
    result = get_first_message(*message, 0);
    if (result != SUCCESS) {
        free(*message);
    }

    return result;
}