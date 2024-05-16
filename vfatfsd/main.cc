/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdio>
#include <string>
#include <unistd.h>
#include <pmos/debug.h>
#include <pthread.h>
#include <thread>
#include <list>
#include <pthread.h>
#include <pmos/system.h>

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

// thread_local Test t;

double count = 0;
std::mutex count_mutex;

thread_local auto pid = get_task_id();


void thread_func(void *) {
    //printf("Hello from a pthread! My PID: %i\n", pid);
    double p = 0;
    for (size_t i = 0; i < 100000000; ++i) {
        asm volatile ("");
        p += i;
    }

    std::lock_guard<std::mutex> lock(count_mutex);
    count += p;
    printf("Count: %li p: %li\n", (uint64_t)count, (uint64_t)p);
}

std::list<std::thread> threads;

extern "C" void test_qsort();
extern "C" void test_pipe();

void run_tests() {
    test_qsort();
    test_pipe();
}

int main() {
    // Sleep is broken
    //sleep(1);
    printf("Starting tests...\n");

    // for (size_t i = 0; i < 10; i++) {
    //     threads.push_back(std::thread(thread_func, nullptr));
    // }

    // for (auto & t : threads) {
    //     t.join();
    // }

    // printf("Count: %li\n", (uint64_t)count);

    //test_qsort();
    test_pipe();

    // Allow other thread to run
    //pthread_exit(nullptr);

    return 0;
}
