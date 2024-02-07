#include "internal_pow.h"
#include "internal_fmod.h"

// Similar to pow()
// Using fast exponentiation by squaring
double __internal_pow(double x, double y) {
    if (y == 0.0) {
        return 1.0;
    }

    if (y == 1.0) {
        return x;
    }

    if (y < 0.0) {
        x = 1.0 / x;
        y = -y;
    }

    double result = 1.0;
    while (y > 1.0) {
        if (__internal_fmod(y, 2.0) == 0.0) {
            x *= x; // Square x
            y /= 2.0; // Halve y
        } else {
            result *= x; // Multiply result by x
            x *= x; // Square x
            y = (y - 1.0) / 2.0; // Halve (y - 1) and update y
        }
    }

    return result * x;
}

// Similar to powf()
float __internal_powf(float x, float y) {
    if (y == 0.0f) {
        return 1.0f;
    }

    if (y == 1.0f) {
        return x;
    }

    if (y < 0.0f) {
        x = 1.0f / x;
        y = -y;
    }

    float result = 1.0f;
    while (y > 1.0f) {
        if (__internal_fmodf(y, 2.0f) == 0.0f) {
            x *= x; // Square x
            y /= 2.0f; // Halve y
        } else {
            result *= x; // Multiply result by x
            x *= x; // Square x
            y = (y - 1.0f) / 2.0f; // Halve (y - 1) and update y
        }
    }

    return result * x;
}

// Similar to powl()
long double __internal_powl(long double x, long double y) {
    if (y == 0.0L) {
        return 1.0L;
    }

    if (y == 1.0L) {
        return x;
    }

    if (y < 0.0L) {
        x = 1.0L / x;
        y = -y;
    }

    long double result = 1.0L;
    while (y > 1.0L) {
        if (__internal_fmodl(y, 2.0L) == 0.0L) {
            x *= x; // Square x
            y /= 2.0L; // Halve y
        } else {
            result *= x; // Multiply result by x
            x *= x; // Square x
            y = (y - 1.0L) / 2.0L; // Halve (y - 1) and update y
        }
    }

    return result * x;
}