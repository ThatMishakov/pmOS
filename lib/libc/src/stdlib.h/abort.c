__attribute__((noreturn)) void _syscall_exit(int status);

__attribute__((noreturn)) void abort(void)
{
    _syscall_exit(1);
}