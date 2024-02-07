#include <unistd.h>

long __attribute__((visibility("hidden"))) __getpagesize_internal() {
    return 4096;
}

int getpagesize() {
    return __getpagesize_internal();
}