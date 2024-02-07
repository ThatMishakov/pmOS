__attribute__((noreturn)) void _syscall_exit(int status);

/// Destructors
void _fini();

/// Atexit functions
void _atexit_pop_all();
void __call_destructors(void);
void __call_thread_atexit();


__attribute__((noreturn)) void exit(int status)
{
    // TODO: Call atexit functions
    __call_thread_atexit();
    _atexit_pop_all();
    __call_destructors();
    _syscall_exit(status);
}