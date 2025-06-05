// mycelium_runtime.h
#pragma once

#include <stddef.h> // For size_t
#include <stdbool.h> // For bool type in C
#include <stdint.h>  // For int32_t, uint32_t

#ifdef __cplusplus
extern "C" {
#endif

// --- Object Header for ARC ---
typedef struct {
    int32_t ref_count; // Reference count
    uint32_t type_id;  // Simple type identifier
} MyceliumObjectHeader;

// --- MyceliumString Structure (Existing) ---
typedef struct {
    char* data;      // Null-terminated character array
    size_t length;   // Number of characters (excluding null terminator)
    size_t capacity; // Allocated buffer size (data buffer size, including space for null terminator)
} MyceliumString;

// --- ARC Runtime Function Declarations ---
MyceliumObjectHeader* Mycelium_Object_alloc(size_t data_size, uint32_t type_id);
void Mycelium_Object_retain(MyceliumObjectHeader* obj_header);
void Mycelium_Object_release(MyceliumObjectHeader* obj_header);
int32_t Mycelium_Object_get_ref_count(MyceliumObjectHeader* obj_header); // For debugging

// --- MyceliumString Runtime Function Declarations (Existing) ---

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

// --- String Conversion Functions ---

// Convert primitive types to MyceliumString
MyceliumString* Mycelium_String_from_int(int val);
MyceliumString* Mycelium_String_from_long(long long val);
MyceliumString* Mycelium_String_from_float(float val);
MyceliumString* Mycelium_String_from_double(double val);
MyceliumString* Mycelium_String_from_bool(bool val);
MyceliumString* Mycelium_String_from_char(char val);


// Convert MyceliumString to primitive types
// Note: These functions should define behavior for invalid conversions (e.g., return 0, false, or handle errors).
int Mycelium_String_to_int(MyceliumString* str);
long long Mycelium_String_to_long(MyceliumString* str);
float Mycelium_String_to_float(MyceliumString* str);
double Mycelium_String_to_double(MyceliumString* str);
bool Mycelium_String_to_bool(MyceliumString* str); // e.g., "true" -> true, others -> false
char Mycelium_String_to_char(MyceliumString* str); // e.g., takes the first character, or 0 if empty/error

// --- Additional String Functions for Primitive Struct Support ---

// Get the length of a MyceliumString (for string.Length property)
int Mycelium_String_get_length(MyceliumString* str);

// Get substring from a MyceliumString starting at the given index
MyceliumString* Mycelium_String_substring(MyceliumString* str, int startIndex);

// Get an empty MyceliumString (for string.Empty static property)
MyceliumString* Mycelium_String_get_empty(void);

// --- Basic Print Utilities ---
void print_int(int val);
void print_double(double val);
void print_bool(bool val);


#ifdef __cplusplus
} // extern "C"
#endif
