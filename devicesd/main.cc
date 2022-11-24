#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <string.h>
#include <system.h>
#include <stdio.h>

int main(int argc, char** argv) {
    char msg[] = "Hello from devicesd!\n";
    send_message_port(1, strlen(msg), msg);
    
    char buff[256];
    sprintf(buff, "argc %i\n", argc);
    send_message_port(1, strlen(buff), buff);
    
    for (int i = 0; i < argc; ++i) {
        sprintf(buff, "Arg %i: %s\n", i, argv[i]);
        send_message_port(1, strlen(buff), buff);
    }

    return 0;
}