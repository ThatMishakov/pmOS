#include <deque>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <pmos/debug.h>
#include <pthread.h>

const thread_local std::deque<std::string> v = {"Hello", "from", "vfatfsd!"};

class Test {
public:
    Test() {
        printf("Test::Test()\n");
    }
    ~Test() {
        printf("Test::~Test()\n");
    }
};

thread_local Test t;

void * thread_func(void * arg) {
    printf("Hello from a pthread!\n");
    return nullptr;
}

int main() {
    for(auto i : v) {
        printf("%s ", i.c_str());
    }
    printf("\n");

    // pid_t pid = fork();
    // if(pid != 0) {
    //     printf("I'm the child!\n");
    // } else {
    //     printf("I'm the parent!\n");
    // }

    pthread_t thread;
    pthread_create(&thread, nullptr, thread_func, nullptr);

    // Allow other thread to run
    pthread_exit(nullptr);

    return 0;
}