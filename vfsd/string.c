#include "string.h"
#include <stdlib.h>
#include <string.h>

int init_null_string(struct String *string) {
    string->data = NULL;
    string->length = 0;
    string->capacity = 0;
    return 0;
}

void destroy_string(struct String *string) {
    if (string->data != NULL)
        free(string->data);
    
    string->data = NULL;
    string->length = 0;
    string->capacity = 0;
}

int append_string(struct String *string, const char *data, size_t length) {
    size_t required_capacity = string->length + length + 1;
    if (required_capacity > string->capacity) {
        size_t new_capacity = (required_capacity > string->capacity * 2) ? required_capacity : string->capacity * 2;
        char *new_data = (char *)realloc(string->data, new_capacity);
        if (new_data == NULL)
            return -1;
        string->data = new_data;
        string->capacity = new_capacity;
    }
    memcpy(string->data + string->length, data, length);
    string->length += length;
    string->data[string->length] = '\0';
    return 0;
}

void erase_string(struct String *string, size_t start, size_t count) {
    if (start >= string->length)
        return;
    if (start + count > string->length)
        count = string->length - start;
    memmove(string->data + start, string->data + start + count, string->length - start - count);
    string->length -= count;
    string->data[string->length] = '\0';
}