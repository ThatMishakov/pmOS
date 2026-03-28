float __internal_floorf(float x);
double __internal_floor(double x);
long double __internal_floorl(long double x);

float floorf(float x) {
    return __internal_floorf(x);
}

double floor(double x) {
    return __internal_floor(x);
}

// long double floorl(long double x) {
//     return __internal_floorl(x);
// }