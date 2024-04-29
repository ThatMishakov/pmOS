#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

void writer_thread(void *arg)
{
    int *pipefd = (int *)arg;
    write(pipefd[1], "hello", 5);
    close(pipefd[1]);
}

void test_pipe()
{
    printf("Testing pipe...\n");

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    char buf[100];
    int nbytes;

    // Create a thread to write to the pipe
    pthread_t writer_thread;
    pthread_create(&writer_thread, NULL, writer_thread, pipefd);

    // Read from the pipe
    nbytes = read(pipefd[0], buf, sizeof(buf));
    close(pipefd[0]);

    printf("Read %d bytes: %s\n", nbytes, buf);
}