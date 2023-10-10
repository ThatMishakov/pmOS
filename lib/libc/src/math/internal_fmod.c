#include "internal_fmod.h"
#include "internal_floor.h"

double __internal_fmod(double x, double y) {
    if (y == 0.0) {
        // Handling division by zero; you can choose your own error handling here.
        // Returning NaN as an example.
        return 0.0 / 0.0; // NaN
    }

    double quotient = x / y;
    double whole_part = __internal_floor(quotient); // Get the integer part of the quotient
    double remainder = x - (whole_part * y);

    return remainder;
}

float __internal_fmodf(float x, float y) {
    if (y == 0.0f) {
        // Handling division by zero; you can choose your own error handling here.
        // Returning NaN as an example.
        return 0.0f / 0.0f; // NaN
    }

    float quotient = x / y;
    float whole_part = __internal_floorf(quotient); // Get the integer part of the quotient
    float remainder = x - (whole_part * y);

    return remainder;
}

long double __internal_fmodl(long double x, long double y) {
    if (y == 0.0L) {
        // Handling division by zero; you can choose your own error handling here.
        // Returning NaN as an example.
        return 0.0L / 0.0L; // NaN
    }

    long double quotient = x / y;
    long double whole_part = __internal_floorl(quotient); // Get the integer part of the quotient
    long double remainder = x - (whole_part * y);

    return remainder;
}