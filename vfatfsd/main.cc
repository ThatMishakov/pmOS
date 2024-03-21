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
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

thread_local auto pid = getpid();

thread_local int test = 1234;

void * thread_func(void * arg) {
    printf("pthread test: %i\n", test);
    printf("Hello from a pthread! My PID: %i\n", pid);
    for (size_t i = 0; i < 100; ++i) {
        pthread_mutex_lock(&count_mutex);
        //printf("Thread %i: %d\n", pid, (uint64_t)count);
        count += i;
        pthread_mutex_unlock(&count_mutex);
    }
    return nullptr;
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

    printf("Count: %d\n", (uint64_t)count);

    // Allow other thread to run
    pthread_exit(nullptr);

    return 0;
}
