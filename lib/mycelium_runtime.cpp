// mycelium_runtime.cpp
#include "mycelium_runtime.h"
#include <string.h> // For strlen, strcpy, memcpy, strcat
#include <stdlib.h> // For malloc, free, realloc
#include <iostream> // For Mycelium_String_print

// Initial capacity helper (can be tuned)
#define MYCELIUM_STRING_INITIAL_CAPACITY 16

MyceliumString* Mycelium_String_new_from_literal(const char* c_str, size_t len) {
    MyceliumString* new_s = (MyceliumString*)malloc(sizeof(MyceliumString));
    if (!new_s) return NULL;

    new_s->length = len;
    // Capacity: ensure enough space, use initial capacity if len is small
    new_s->capacity = (len == 0) ? 0 : (len < MYCELIUM_STRING_INITIAL_CAPACITY ? MYCELIUM_STRING_INITIAL_CAPACITY : len + 1);
    
    if (new_s->capacity > 0) {
        new_s->data = (char*)malloc(new_s->capacity);
        if (!new_s->data) {
            free(new_s);
            return NULL;
        }
        memcpy(new_s->data, c_str, len);
        new_s->data[len] = '\0';
    } else { // Empty string
        new_s->data = NULL; // Or point to a global static empty string ""
    }
    return new_s;
}

MyceliumString* Mycelium_String_concat(MyceliumString* s1, MyceliumString* s2) {
    if (!s1 || !s1->data) { // Treat null s1 as empty string for concatenation
        if (!s2 || !s2->data) return Mycelium_String_new_from_literal("", 0); // Both null/empty
        return Mycelium_String_new_from_literal(s2->data, s2->length); // s1 null/empty, s2 valid
    }
    if (!s2 || !s2->data) { // s1 valid, s2 null/empty
         return Mycelium_String_new_from_literal(s1->data, s1->length);
    }

    size_t new_len = s1->length + s2->length;
    MyceliumString* result_s = (MyceliumString*)malloc(sizeof(MyceliumString));
    if (!result_s) return NULL;

    result_s->length = new_len;
    result_s->capacity = (new_len < MYCELIUM_STRING_INITIAL_CAPACITY ? MYCELIUM_STRING_INITIAL_CAPACITY : new_len + 1);
    result_s->data = (char*)malloc(result_s->capacity);

    if (!result_s->data) {
        free(result_s);
        return NULL;
    }

    memcpy(result_s->data, s1->data, s1->length);
    memcpy(result_s->data + s1->length, s2->data, s2->length);
    result_s->data[new_len] = '\0';

    return result_s;
}

void Mycelium_String_print(MyceliumString* str) {
    if (str && str->data) {
        std::cout << str->data;
    } else if (str == NULL) {
        // Decide how to print a null MyceliumString*
        // std::cout << "(null string ptr)";
    } else { // str is not NULL, but str->data is NULL (empty string)
        // Empty string prints nothing, which is fine.
    }
}

void Mycelium_String_delete(MyceliumString* str) {
    if (str) {
        free(str->data); // free(NULL) is safe
        free(str);
    }
}