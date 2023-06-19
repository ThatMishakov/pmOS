#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include "header.h"

void check_args(int argc, char *argv[])
{
    
}

char * get_filename(int argc, char *argv[])
{
    assert(argc >=2 && "mising argument");

    return argv[1];
}

char * filename = NULL;
int archive_fd = -1;

void parse_headers()
{
    size_t header_offset = 0;
    


}

int main(int argc, char *argv[])
{
    check_args(argc, argv);

    filename = get_filename(argc, argv);
    
    int archive_fd = open(filename, O_RDONLY);
    if (archive_fd < 0) {
        perror("error in open()");
    }

    parse_headers();


    return 0;
}