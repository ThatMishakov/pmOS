__attribute__((noreturn)) void _syscall_exit(int status);

/// Destructors
void _fini();

/// Atexit functions
void _atexit_pop_all();

void __call_destructors(void);


__attribute__((noreturn)) void exit(int status)
{
    // TODO: Call atexit functions
    _atexit_pop_all();
    __call_destructors();
    _syscall_exit(status);
}