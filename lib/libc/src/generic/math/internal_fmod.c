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

// long double __internal_fmodl(long double x, long double y) {
//     if (y == 0.0L) {
//         // Handling division by zero; you can choose your own error handling here.
//         // Returning NaN as an example.
//         return 0.0L / 0.0L; // NaN
//     }

//     long double quotient = x / y;
//     long double whole_part = __internal_floorl(quotient); // Get the integer part of the quotient
//     long double remainder = x - (whole_part * y);

//     return remainder;
// }