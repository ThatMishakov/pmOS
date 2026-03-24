#include <stdint.h>

int main(int argc, char **argv);
void exit(int status);
void __call_init_functions();

int __main(int argc, char **argv, char **envp) __attribute__((weak));

extern int __argc;
extern char **__argv;
extern char **__envp;

void *main_trampoline(void *)
{
    __call_init_functions();
    int result = __main(__argc, __argv, __envp);
    exit(result);
}