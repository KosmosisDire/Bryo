// mycelium_runtime.cpp
#include "mycelium_runtime.h"
#include <string.h> // For strlen, strcpy, memcpy, strcat
#include <stdlib.h> // For malloc, free, realloc, strtol, strtoll, strtof, strtod
#include <iostream> // For Mycelium_String_print
#include <string>   // For std::to_string, std::string
#include <vector>   // For std::vector in C++ part of conversion
#include <algorithm> // for std::transform for to_bool

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

// --- String Conversion Function Implementations ---

// Helper to convert std::string to MyceliumString*
// This is an internal helper, not exposed via extern "C" directly unless needed
MyceliumString* internal_cpp_string_to_mycelium_string(const std::string& cpp_str) {
    return Mycelium_String_new_from_literal(cpp_str.c_str(), cpp_str.length());
}

MyceliumString* Mycelium_String_from_int(int val) {
    return internal_cpp_string_to_mycelium_string(std::to_string(val));
}

MyceliumString* Mycelium_String_from_long(long long val) {
    return internal_cpp_string_to_mycelium_string(std::to_string(val));
}

MyceliumString* Mycelium_String_from_float(float val) {
    // std::to_string for float can have precision issues or fixed notation.
    // For better control, one might use snprintf.
    std::string s = std::to_string(val);
    // Remove trailing zeros for floats, e.g., "3.140000" -> "3.14"
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (s.back() == '.') {
        s.pop_back();
    }
    return internal_cpp_string_to_mycelium_string(s);
}

MyceliumString* Mycelium_String_from_double(double val) {
    std::string s = std::to_string(val);
    // Remove trailing zeros for doubles
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (s.back() == '.') {
        s.pop_back();
    }
    return internal_cpp_string_to_mycelium_string(s);
}

MyceliumString* Mycelium_String_from_bool(bool val) {
    return internal_cpp_string_to_mycelium_string(val ? "true" : "false");
}

MyceliumString* Mycelium_String_from_char(char val) {
    char str[2] = {val, '\0'};
    return Mycelium_String_new_from_literal(str, 1);
}

// Convert MyceliumString to primitive types
int Mycelium_String_to_int(MyceliumString* str) {
    if (!str || !str->data || str->length == 0) return 0; // Or some error indicator
    char* endptr;
    long val = strtol(str->data, &endptr, 10);
    // Check if conversion was successful and consumed the whole string (or up to non-numeric)
    if (endptr == str->data) return 0; // No conversion happened
    // Check for overflow/underflow if strtol sets errno, or if val is LONG_MAX/LONG_MIN
    // For simplicity, directly casting to int here.
    return (int)val;
}

long long Mycelium_String_to_long(MyceliumString* str) {
    if (!str || !str->data || str->length == 0) return 0LL;
    char* endptr;
    long long val = strtoll(str->data, &endptr, 10);
    if (endptr == str->data) return 0LL;
    return val;
}

float Mycelium_String_to_float(MyceliumString* str) {
    if (!str || !str->data || str->length == 0) return 0.0f;
    char* endptr;
    float val = strtof(str->data, &endptr);
    if (endptr == str->data) return 0.0f;
    return val;
}

double Mycelium_String_to_double(MyceliumString* str) {
    if (!str || !str->data || str->length == 0) return 0.0;
    char* endptr;
    double val = strtod(str->data, &endptr);
    if (endptr == str->data) return 0.0;
    return val;
}

bool Mycelium_String_to_bool(MyceliumString* str) {
    if (!str || !str->data) return false;
    // Simple case-insensitive "true" check
    std::string s(str->data, str->length);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s == "true";
}

char Mycelium_String_to_char(MyceliumString* str) {
    if (!str || !str->data || str->length == 0) return '\0';
    return str->data[0]; // Return the first character
}

// --- ARC Function Implementations ---

MyceliumObjectHeader* Mycelium_Object_alloc(size_t data_size, uint32_t type_id) {
    size_t total_size = sizeof(MyceliumObjectHeader) + data_size;
    MyceliumObjectHeader* header_ptr = (MyceliumObjectHeader*)malloc(total_size);
    if (!header_ptr) {
        // Consider logging an error or using a more robust error handling mechanism
        // For now, returning NULL indicates failure.
        return NULL;
    }
    header_ptr->ref_count = 1;
    header_ptr->type_id = type_id;
    // The memory for data_size immediately follows the header.
    // The caller will typically cast (header_ptr + 1) to their specific object type.
    return header_ptr;
}

void Mycelium_Object_retain(MyceliumObjectHeader* obj_header) {
    if (obj_header != NULL) {
        // Future: Add atomic increment for thread safety if Sharpie supports concurrency.
        // For now, simple increment is fine for single-threaded or externally synchronized scenarios.
        obj_header->ref_count++;
    }
}

void Mycelium_Object_release(MyceliumObjectHeader* obj_header) {
    if (obj_header != NULL) {
        // Future: Add atomic decrement for thread safety.
        obj_header->ref_count--;
        if (obj_header->ref_count == 0) {
            // Future: Call object's destructor based on type_id before freeing.
            // For this simple slice, we just free the memory.
            free(obj_header); // Frees the entire block (header + data).
        }
        // Optional: Add a log here if ref_count goes below zero (indicates a bug).
    }
}

int32_t Mycelium_Object_get_ref_count(MyceliumObjectHeader* obj_header) {
    if (obj_header != NULL) {
        return obj_header->ref_count;
    }
    return -1; // Or some other error indicator (e.g., 0 if null implies no refs)
}

// --- Basic Print Utilities Implementation ---
void print_int(int val) {
    std::cout << val; // Simple print to cout
}

void print_double(double val) {
    std::cout << val; // Simple print to cout
}

void print_bool(bool val) {
    std::cout << (val ? "true" : "false"); // Print "true" or "false"
}

// --- Additional String Functions for Primitive Struct Support ---

int Mycelium_String_get_length(MyceliumString* str) {
    if (!str) return 0;
    return (int)str->length;
}

MyceliumString* Mycelium_String_substring(MyceliumString* str, int startIndex) {
    if (!str || !str->data || startIndex < 0 || startIndex >= (int)str->length) {
        return Mycelium_String_new_from_literal("", 0); // Return empty string for invalid input
    }
    
    size_t remaining_length = str->length - startIndex;
    return Mycelium_String_new_from_literal(str->data + startIndex, remaining_length);
}

MyceliumString* Mycelium_String_get_empty(void) {
    return Mycelium_String_new_from_literal("", 0);
}
