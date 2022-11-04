#include <stdlib.h>
#include <kernel/types.h>

extern "C" int main() {
    char* k = (char*)malloc(GB(1));
    k[MB(100)] = 1;
    k[MB(200)] = 2;
    k[MB(300)] = 3;

    return 0;
}