#include <math.h>

int isinf(double x) {
    return x == HUGE_VAL || x == -HUGE_VAL;
}