#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

// void *writer_thread(void *arg)
// {
//     int *pipefd = (int *)arg;
//     printf("Writing...\n");

//     int fd2 = dup(pipefd[1]);
//     if (fd2 == -1) {
//         perror("dup");
//         return NULL;
//     }
//     for (int i = 0; i < 3; ++i) {
//         int result = write(pipefd[1], "hello1!", 6);
//         printf("Wrote %d bytes\n", result);
//         if (result == -1) {
//             perror("write");
//         }
//         printf("Writing to fd2... %i\n", fd2);
//         result = write(fd2, "hello2!", 6);
//         printf("Wrote %d bytes\n", result);
//         if (result == -1) {
//             perror("write");
//         }
//         sleep(1);
//     }
//     close(pipefd[1]);
//     close(fd2);
//     return NULL;
// }

#include <errno.h>
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

    pid_t pid = fork();
    if (pid == 0) {
        printf("Child: my pid: %li\n", getpid());
        // TODO: investigate getppid() and getpgrp() not working
        printf("Writing...\n");
        close(pipefd[0]);

        int fd2 = dup(pipefd[1]);
        if (fd2 == -1) {
            perror("dup");
            return;
        }
        for (int i = 0; i < 3; ++i) {
            int result = write(pipefd[1], "hello1!", 6);
            printf("Wrote %d bytes\n", result);
            if (result == -1) {
                perror("write");
            }
            printf("Writing to fd2... %i\n", fd2);
            result = write(fd2, "hello2!", 6);
            printf("Wrote %d bytes\n", result);
            if (result == -1) {
                perror("write");
            }
            sleep(1);
        }
        close(pipefd[1]);
        close(fd2);
        return;
    } else if (pid == -1) {
        perror("fork");
        return;
    } else {
        printf("Forked! Child PID: %i\n", pid);
    }
    close(pipefd[1]);

    // Read from the pipe
    while ((nbytes = read(pipefd[0], buf, sizeof(buf))) > 0) {
        printf("Read %d bytes: %s\n", nbytes, buf);
    }
    close(pipefd[0]);

    // Create a thread to write to the pipe
    // printf("Creating writer thread...\n");
    // pthread_t writer_thread_;
    // pthread_create(&writer_thread_, NULL, writer_thread, pipefd);
    // pthread_detach(writer_thread_);
}