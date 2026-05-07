#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

const char path[] = "/root/test_file.txt";

void read_test_file()
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open test file");
        return;
    }

    printf("Opened %s!\n", path);

    char buffer[256];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        perror("Failed to read from test file");
        close(fd);
        return;
    }

    buffer[bytes_read] = '\0';
    printf("Contents of test_file.txt:\n%s\n", buffer);

    close(fd);
}