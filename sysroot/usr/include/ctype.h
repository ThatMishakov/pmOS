#ifndef _CTYPE_H
#define _CTYPE_H

#if defined(__cplusplus)
extern "C" {
#endif

int isalnum(int c);
int isalpha(int c);

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
 * i.e., any character with ASCII code less than 32 (space) or the delete (DEL) character (ASCII 127).
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

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _CTYPE_H */