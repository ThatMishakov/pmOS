#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <string.h>
#include <system.h>

extern "C" int main(int argc, char** argv) {
    char msg[] = "Hello from devicesd!\n";
    send_message_port(1, strlen(msg), msg);

    return 0;
}