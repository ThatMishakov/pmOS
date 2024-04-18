double __internal_fmod(double x, double y);
float __internal_fmodf(float x, float y);
long double __internal_fmodl(long double x, long double y);

double fmod(double x, double y) {
    return __internal_fmod(x, y);
}

float fmodf(float x, float y) {
    return __internal_fmodf(x, y);
}

long double fmodl(long double x, long double y) {
    return __internal_fmodl(x, y);
}