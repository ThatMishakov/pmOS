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

#ifndef __LOCALE_H
#define __LOCALE_H

#define __DECLARE_LOCALE_T
#include "__posix_types.h"

#define LC_ALL 0
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5

struct lconv {
    char *decimal_point; // "."
    char *thousands_sep; // ""
    char *grouping; // ""
    char *mon_decimal_point; // ""
    char *mon_thousands_sep; // ""
    char *mon_grouping; // ""
    char *positive_sign; // ""
    char *negative_sign; // ""
    char *currency_symbol; // ""
    char frac_digits; // CHAR_MAX
    char p_cs_precedes; // CHAR_MAX
    char n_cs_precedes; // CHAR_MAX
    char p_sep_by_space; // CHAR_MAX
    char n_sep_by_space; // CHAR_MAX
    char p_sign_posn; // CHAR_MAX
    char n_sign_posn; // CHAR_MAX
    char *int_curr_symbol; // ""
    char int_frac_digits; // CHAR_MAX
    char int_p_cs_precedes; // CHAR_MAX
    char int_n_cs_precedes; // CHAR_MAX
    char int_p_sep_by_space; // CHAR_MAX
    char int_n_sep_by_space; // CHAR_MAX
    char int_p_sign_posn; // CHAR_MAX
    char int_n_sign_posn; // CHAR_MAX
};

#if defined(__cplusplus)
extern "C" {
#endif

char *setlocale(int category, const char *locale);
struct lconv *localeconv(void);


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* __LOCALE_H */