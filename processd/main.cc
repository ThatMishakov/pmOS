#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <string.h>
#include <pmos/system.h>

__thread char test2[1];

int main() {
    char msg[] = "Hello from processd!\n";
    send_message_port(1, strlen(msg), msg);

    return 0;
}