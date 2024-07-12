#include <stdint.h>

typedef struct {
    int argc;
    char **argv;
} Args_Ret;

Args_Ret prepare_args(uint64_t argtype, uint64_t ptr);
int main(int argc, char **argv);
void exit(int status);
void __call_init_functions();

int __main(int argc, char **argv);

void *main_trampoline(void *)
{
    __call_init_functions();
    Args_Ret args = prepare_args(0, 0);
    int result = __main(args.argc, args.argv);
    exit(result);
}