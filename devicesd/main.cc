#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <string.h>
#include <system.h>
#include <stdio.h>

int main(int argc, char** argv) {
    char msg[] = "Hello from devicesd!\n";
    send_message_port(1, strlen(msg), msg);

    for (int i = 0; i < argc; ++i) {
        char buff[256];
        sprintf(buff, "Arg %i: %s\n", i, argv[i]);
        send_message_port(1, strlen(buff), buff);
    }
    send_message_port(1, t, buff);

    return 0;
}