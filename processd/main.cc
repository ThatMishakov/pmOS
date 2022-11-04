#include <stdlib.h>

extern "C" int main() {
    int* t = (int*)malloc(sizeof(int));
    *t = 123;

    return 0;
}