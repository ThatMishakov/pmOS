/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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