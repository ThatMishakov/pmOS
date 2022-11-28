#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <string.h>
#include <system.h>
#include <stdio.h>

char* exec = NULL;

void usage()
{

}

void parse_args(int argc, char** argv)
{
    if (argc < 1) return;
    exec = argv[0];

    for (int i = 1; i < argc; ++i) {
        printf("argc %i %s\n", i, argv[i]);
    }
}

int main(int argc, char** argv) {
    char msg[] = "Hello from devicesd!\n";
    send_message_port(1, strlen(msg), msg);
    
    parse_args(argc, argv);

    return 0;
}