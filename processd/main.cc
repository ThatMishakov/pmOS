#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <string.h>
#include <system.h>

extern "C" int main() {
    char msg[] = "Hello from processd!\n";
    send_message_port(1, strlen(msg), msg);

    uint64_t* test = (uint64_t*)0xfacade;
    *test = 0;

    return 0;
}