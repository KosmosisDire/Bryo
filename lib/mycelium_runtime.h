// mycelium_runtime.h
#pragma once

#include <stddef.h> // For size_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* data;      // Null-terminated character array
    size_t length;   // Number of characters (excluding null terminator)
    size_t capacity; // Allocated buffer size (data buffer size, including space for null terminator)
} MyceliumString;

// --- Runtime Function Declarations ---

// Creates a new MyceliumString from a C string literal.
// The runtime takes ownership of the new string's memory.
MyceliumString* Mycelium_String_new_from_literal(const char* c_str, size_t len);

// Concatenates two MyceliumStrings, returning a new MyceliumString.
// The runtime takes ownership of the new string's memory. s1 and s2 are not modified.
MyceliumString* Mycelium_String_concat(MyceliumString* s1, MyceliumString* s2);

// Prints a MyceliumString to standard output (example utility).
void Mycelium_String_print(MyceliumString* str);

// Deallocates a MyceliumString (important for memory management).
void Mycelium_String_delete(MyceliumString* str);

#ifdef __cplusplus
} // extern "C"
#endif