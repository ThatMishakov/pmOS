#include <stdlib.h>

void *bsearch(const void *key, const void *base, size_t nel, size_t width, int (*cmp)(const void *keyval, const void *datum)) {
    const char *base_ptr = (const char *)base;
    size_t low = 0;
    size_t high = nel;

    while (low < high) {
        size_t mid = (low + high) / 2;
        int cmp_result = cmp(key, base_ptr + mid * width);
        if (cmp_result < 0) {
            high = mid;
        } else if (cmp_result > 0) {
            low = mid + 1;
        } else {
            return (void *)(base_ptr + mid * width);
        }
    }

    return NULL;
}