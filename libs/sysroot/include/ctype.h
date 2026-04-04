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

#ifndef _CTYPE_H
#define _CTYPE_H

#define __DECLARE_LOCALE_T
#include "__posix_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

int isalnum(int c);
int isalpha(int c);
int isascii(int);

/**
 * @brief Check if a character is a blank character.
 *
 * The `isblank` function checks whether the given character `c` is a blank character,
 * i.e., either a space (' ') or a tab ('\t').
 *
 * @param c The character to check.
 * @return Nonzero value (true) if `c` is a blank character, 0 (false) otherwise.
 */
int isblank(int c);

/**
 * @brief Check if a character is a control character.
 *
 * The `iscntrl` function checks whether the given character `c` is a control character,
 * i.e., any character with ASCII code less than 32 (space) or the delete (DEL) character (ASCII
 * 127).
 *
 * @param c The character to check.
 * @return Nonzero value (true) if `c` is a control character, 0 (false) otherwise.
 */
int iscntrl(int c);

int isdigit(int c);
int isgraph(int c);
int islower(int c);

/**
 * @brief Check if a character is a printable character.
 *
 * The `isprint` function checks whether the given character `c` is a printable character,
 * i.e., any character that can be printed (including space and punctuation characters).
 *
 * @param c The character to check.
 * @return Nonzero value (true) if `c` is a printable character, 0 (false) otherwise.
 */
int isprint(int c);

/**
 * @brief Check if a character is a punctuation character.
 *
 * The `ispunct` function checks whether the given character `c` is a punctuation character,
 * i.e., any printable character that is not alphanumeric or a space.
 *
 * @param c The character to check.
 * @return Nonzero value (true) if `c` is a punctuation character, 0 (false) otherwise.
 */
int ispunct(int c);

int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);
int toascii(int);

int isalnum_l(int, locale_t);
int isalpha_l(int, locale_t);
int isblank_l(int, locale_t);
int iscntrl_l(int, locale_t);
int isdigit_l(int, locale_t);
int isgraph_l(int, locale_t);
int islower_l(int, locale_t);
int isprint_l(int, locale_t);
int ispunct_l(int, locale_t);
int isspace_l(int, locale_t);
int isupper_l(int, locale_t);
int isxdigit_l(int, locale_t);
int tolower_l(int, locale_t);
int toupper_l(int, locale_t);

#define _toupper toupper
#define _tolower tolower

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _CTYPE_H */