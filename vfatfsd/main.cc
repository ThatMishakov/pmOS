#include <cstdio>
#include <string>
#include <unistd.h>
#include <pmos/debug.h>
#include <pthread.h>
#include <thread>
#include <list>
#include <pthread.h>

class Test {
public:
    Test(const std::string & msg) {
        printf("Test::Test(char * msg): %s\n", msg.c_str());
    }

    Test() {
        printf("Test::Test()\n");
    }
    ~Test() {
        printf("Test::~Test()\n");
    }
};

// Check global constructors
Test tt("Global constructor test");

thread_local Test t;

double count = 0;
std::mutex count_mutex;

thread_local auto pid = getpid();


void thread_func(void *) {
    printf("Hello from a pthread! My PID: %i\n", pid);
    double p = 0;
    for (size_t i = 0; i < 10000000; ++i) {
        asm volatile ("");
        p += i;
    }

    std::lock_guard<std::mutex> lock(count_mutex);
    count += p;
    printf("Count: %li p: %li\n", (uint64_t)count, (uint64_t)p);
}

std::list<std::thread> threads;

int main() {
    // Sleep is broken
    //sleep(1);
    printf("Starting tests...\n");

    for (size_t i = 0; i < 10; i++) {
        threads.push_back(std::thread(thread_func, nullptr));
    }

    for (auto & t : threads) {
        t.join();
    }

    printf("Count: %li\n", (uint64_t)count);

    // Allow other thread to run
    pthread_exit(nullptr);

    return 0;
}
