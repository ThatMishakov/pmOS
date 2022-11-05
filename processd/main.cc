#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <string.h>
#include <system.h>

extern "C" int main() {
    char msg[] = "Hello from loader!\n";
    send_message_task(0, 0, strlen(msg), msg);

    return 0;
}