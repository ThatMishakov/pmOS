#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

static void *thread_func(void *arg)
{
    while (10000000)
    {
        // Do nothing
        __asm__("");
    }
}

void test_tlb_shootdown()
{
    // mmap a region, access it (so the pages are allocated), create a bunch of threads, and unmap it
    // to see TLB shootdown in kernel logs

    int number_of_pages = 10;
    void *addr = mmap(NULL, 4096*10, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    for (int i = 0; i < number_of_pages/2; i++)
    {
        ((char *)addr)[i * 4096*2] = 0;
    }

    pthread_t threads[4];
    for (int i = 0; i < 4; i++)
    {
        pthread_create(&threads[i], NULL, thread_func, NULL);
        pthread_detach(threads[i]);
    }

    usleep(10);

    munmap(addr, 4096*10);
}