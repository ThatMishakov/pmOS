#ifndef STRING_H
#define STRING_H
#include <stddef.h>

/**
 * @struct String
 * @brief Represents a dynamically resizable string.
 *
 * The `String` struct represents a string that can dynamically grow in size as needed.
 * It stores a character buffer (`data`), the current length of the string (`length`),
 * and the allocated capacity of the buffer (`capacity`). The buffer is always null-terminated
 * but can contain null characters in the middle of the string.
 *
 * @var String::data
 * Pointer to the character buffer of the string.
 *
 * @var String::length
 * The current length of the string.
 *
 * @var String::capacity
 * The allocated capacity of the character buffer.
 */
struct String {
    char *data;
    size_t length;
    size_t capacity;
};


/**
 * @brief Initializes an empty string.
 *
 * This function initializes the given string with null values, indicating an empty string.
 * The string's data pointer is set to NULL, the length is set to 0, and the capacity is set to 0.
 *
 * @param string Pointer to the string to be initialized.
 * @return 0 on success, -1 on failure.
 */
int init_null_string(struct String *string);


/**
 * @brief Destroys a string.
 *
 * This function frees the memory buffer of the string and sets the string's data pointer to NULL.
 * The length and capacity of the string are set to 0.
 *
 * @param string Pointer to the string to be destroyed.
 */
void destroy_string(struct String *string);


/**
 * @brief Appends characters to the end of a string.
 *
 * This function appends the specified characters to the end of the given string.
 * It dynamically allocates memory if needed to accommodate the new characters.
 * The length and capacity of the string are updated accordingly, and a null terminator
 * is placed at the end of the string.
 *
 * @param string Pointer to the string to append to.
 * @param data   Pointer to the characters (string) to append.
 * @param length The number of characters to append.
 * @return 0 on success, -1 on failure.
 */
int append_string(struct String *string, const char *data, size_t length);


/**
 * @brief Erases a specified number of characters from a string starting at a given position.
 *
 * This function removes the specified number of characters from the string starting at
 * the specified position. The remaining characters are shifted to fill the erased portion.
 * The string length is updated accordingly, and a null terminator is placed at the new end
 * of the string.
 *
 * @param string Pointer to the string to be modified.
 * @param start  The starting position from where characters should be erased.
 * @param count  The number of characters to erase.
 *
 * @note If the starting position is beyond the string length, no changes are made.
 *       If the starting position plus the count exceeds the string length,
 *       the count is adjusted to erase only the available characters.
 */
void erase_string(struct String *string, size_t start, size_t count);


#endif