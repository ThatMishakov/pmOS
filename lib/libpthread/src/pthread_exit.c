#include <stdint.h>

void _atexit_pop_all();
void __call_destructors(void);

// Defined in cxa_thread_atexit.c
void __call_thread_atexit();

extern uint64_t __active_threads;

/// @brief Run destructor functions
///
/// This function gets called from pthread_exit() and __cxa_thread_exit() and runs the __cxa_thread_atexit() functions.
/// If the thread is the last one, it also runs the atexit() (and similar cxa) functions.
void __thread_exit_fire_destructors()
{
    __call_thread_atexit();

    uint64_t remaining_count = __atomic_sub_fetch(&__active_threads, 1, __ATOMIC_SEQ_CST);
    if (remaining_count == 0) {
        // Last thread, call atexit() stuff
        _atexit_pop_all();
        __call_destructors();
    }
}