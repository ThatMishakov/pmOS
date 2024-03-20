#include <cstdio>
#include <string>
#include <unistd.h>
#include <pmos/debug.h>
#include <pthread.h>
#include <thread>
#include <list>

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

void * thread_func(void * arg) {
    printf("Hello from a pthread! My PID: %i\n", getpid());
    return nullptr;
}

std::list<std::thread> threads;

int main() {
    // Sleep is broken
    //sleep(1);
    printf("Starting tests...\n");

    for (size_t i = 0; i < 100; i++) {
        threads.push_back(std::thread(thread_func, nullptr));
    }

    for (auto & t : threads) {
        t.detach();
    }

    // Allow other thread to run
    pthread_exit(nullptr);

    return 0;
}
