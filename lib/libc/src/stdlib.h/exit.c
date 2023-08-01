__attribute__((noreturn)) void _syscall_exit(int status);

/// Destructors
void _fini();

__attribute__((noreturn)) void exit(int status)
{
    // TODO: Call atexit functions

    _fini();
    _syscall_exit(status);
}