// mycelium_runtime.cpp - Version with logging system integration
#include "mycelium_runtime.h"
#include <string.h> // For strlen, strcpy, memcpy, strcat
#include <stdlib.h> // For malloc, free, realloc, strtol, strtoll, strtof, strtod
#include <iostream> // For Mycelium_String_print
#include <string>   // For std::to_string, std::string
#include <vector>   // For std::vector in C++ part of conversion
#include <algorithm> // for std::transform for to_bool
#include <atomic>   // For std::atomic operations (thread-safe reference counting)

// Try to include the logger - this might fail if we're being compiled without it
// In that case, we'll fall back to console output
#ifdef __cplusplus
extern "C" {
#endif
    // Forward declare logger functions for C compatibility
    void runtime_log_debug(const char* message);
    void runtime_log_info(const char* message);
    void runtime_log_warn(const char* message);
#ifdef __cplusplus
}
#endif

// Helper macros for logging from C code
#define LOG_RUNTIME_TRACE(msg) runtime_log_debug(msg)  // Map TRACE to DEBUG level
#define LOG_RUNTIME_DEBUG(msg) runtime_log_debug(msg)
#define LOG_RUNTIME_INFO(msg) runtime_log_info(msg)
#define LOG_RUNTIME_WARN(msg) runtime_log_warn(msg)
#define LOG_RUNTIME_ERROR(msg) runtime_log_warn(msg)   // Map ERROR to WARN level (no error function defined)

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
    // Simple, safe null pointer validation - removed dangerous address range checks
    LOG_RUNTIME_TRACE("Mycelium_String_concat called");
    
    // Handle null string cases safely
    if (!s1) {
        if (!s2) return Mycelium_String_new_from_literal("", 0);
        // Only access s2 fields after confirming s2 is not null
        if (!s2->data) return Mycelium_String_new_from_literal("", 0);
        return Mycelium_String_new_from_literal(s2->data, s2->length);
    }
    
    if (!s2) {
        // Only access s1 fields after confirming s1 is not null
        if (!s1->data) return Mycelium_String_new_from_literal("", 0);
        return Mycelium_String_new_from_literal(s1->data, s1->length);
    }
    
    // Both s1 and s2 are non-null, now safe to access their data fields
    if (!s1->data) {
        if (!s2->data) return Mycelium_String_new_from_literal("", 0);
        return Mycelium_String_new_from_literal(s2->data, s2->length);
    }
    
    if (!s2->data) {
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
    if (str && str->data)
    {
        Mycelium::Scripting::Common::LOG_RUNTIME(std::string(str->data, str->length), "RUNTIME");
    } else if (str == NULL)
    {
        // Decide how to print a null MyceliumString*
        // std::cout << "(null string ptr)";
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

// --- VTable Registry Implementation ---
#include <map>
#include <stdexcept>
#include <set>

// Thread-safety note: In a multi-threaded environment, this would need synchronization
static std::map<uint32_t, MyceliumVTable*> vtable_registry;

// GLOBAL OBJECT TRACKING FOR DEBUGGING DOUBLE-FREE ISSUES (Silent version)
struct ObjectTrackingInfo {
    void* header_ptr;
    uint32_t type_id;
    int ref_count;
    bool is_freed;
    std::string debug_name;
};

static std::map<void*, ObjectTrackingInfo> tracked_objects;
static int next_object_id = 1;

void track_object_allocation(void* header_ptr, uint32_t type_id, const std::string& debug_name = "") {
    ObjectTrackingInfo info;
    info.header_ptr = header_ptr;
    info.type_id = type_id;
    info.ref_count = 1;
    info.is_freed = false;
    info.debug_name = debug_name.empty() ? ("Object_" + std::to_string(next_object_id++)) : debug_name;
    
    tracked_objects[header_ptr] = info;
    
    // Log object allocation to debug file
    std::string alloc_msg = "[OBJECT TRACKER] ALLOCATED: " + std::to_string(reinterpret_cast<uintptr_t>(header_ptr)) + 
                           " (" + info.debug_name + ") type_id=" + std::to_string(type_id) + 
                           " ref_count=" + std::to_string(info.ref_count);
    LOG_RUNTIME_DEBUG(alloc_msg.c_str());
}

void track_object_retain(void* header_ptr) {
    auto it = tracked_objects.find(header_ptr);
    if (it != tracked_objects.end()) {
        it->second.ref_count++;
        // Log object retain to debug file
        std::string retain_msg = "[OBJECT TRACKER] RETAINED: " + std::to_string(reinterpret_cast<uintptr_t>(header_ptr)) + 
                                " (" + it->second.debug_name + ") ref_count=" + std::to_string(it->second.ref_count);
        LOG_RUNTIME_DEBUG(retain_msg.c_str());
    } else {
        std::string warn_msg = "[OBJECT TRACKER] WARNING: Retaining untracked object " + 
                              std::to_string(reinterpret_cast<uintptr_t>(header_ptr));
        LOG_RUNTIME_WARN(warn_msg.c_str());
    }
}

void track_object_release(void* header_ptr) {
    auto it = tracked_objects.find(header_ptr);
    if (it != tracked_objects.end()) {
        if (it->second.is_freed) {
            std::string error_msg = "[OBJECT TRACKER] ERROR: Double-release of ALREADY FREED object " + 
                                   std::to_string(reinterpret_cast<uintptr_t>(header_ptr)) + 
                                   " (" + it->second.debug_name + ")";
            LOG_RUNTIME_WARN(error_msg.c_str());
            return;
        }
        
        it->second.ref_count--;
        std::string release_msg = "[OBJECT TRACKER] RELEASED: " + std::to_string(reinterpret_cast<uintptr_t>(header_ptr)) + 
                                 " (" + it->second.debug_name + ") ref_count=" + std::to_string(it->second.ref_count);
        LOG_RUNTIME_DEBUG(release_msg.c_str());
        
        if (it->second.ref_count == 0) {
            it->second.is_freed = true;
            std::string freed_msg = "[OBJECT TRACKER] FREED: " + std::to_string(reinterpret_cast<uintptr_t>(header_ptr)) + 
                                   " (" + it->second.debug_name + ")";
            LOG_RUNTIME_DEBUG(freed_msg.c_str());
        }
    } else {
        std::string warn_msg = "[OBJECT TRACKER] WARNING: Releasing untracked object " + 
                              std::to_string(reinterpret_cast<uintptr_t>(header_ptr));
        LOG_RUNTIME_WARN(warn_msg.c_str());
    }
}

void dump_tracked_objects() {
    // Debug dump removed for clean console
}

void Mycelium_VTable_register(uint32_t type_id, MyceliumVTable* vtable) {
    if (vtable == nullptr) {
        // Handle error - perhaps log or throw
        return;
    }
    vtable_registry[type_id] = vtable;
}

MyceliumVTable* Mycelium_VTable_get(uint32_t type_id) {
    auto it = vtable_registry.find(type_id);
    if (it != vtable_registry.end()) {
        return it->second;
    }
    return nullptr; // Type not found
}

// --- ARC Function Implementations ---
MyceliumObjectHeader* Mycelium_Object_alloc(size_t data_size, uint32_t type_id, MyceliumVTable* vtable) {
    size_t total_size = sizeof(MyceliumObjectHeader) + data_size;
    MyceliumObjectHeader* header_ptr = (MyceliumObjectHeader*)malloc(total_size);
    if (!header_ptr) {
        // Consider logging an error or using a more robust error handling mechanism
        // For now, returning NULL indicates failure.
        return NULL;
    }
    
    // Properly construct the atomic ref_count using placement new
#ifdef __cplusplus
    new (&header_ptr->ref_count) std::atomic<int32_t>(1);
#else
    header_ptr->ref_count = 1;
#endif
    header_ptr->type_id = type_id;
    header_ptr->vtable = vtable;
    
    // Zero-initialize the data section to prevent garbage values in fields
    if (data_size > 0) {
        void* data_ptr = (void*)(header_ptr + 1);
        memset(data_ptr, 0, data_size);
    }
    
    // Log debug info about object allocation
    std::string debug_msg = "[DEBUG] Mycelium_Object_alloc:\n  header_ptr: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(header_ptr)) +
                           "\n  type_id: " + std::to_string(type_id) +
                           "\n  vtable: " + std::to_string(reinterpret_cast<uintptr_t>(vtable));
    if (vtable) {
        debug_msg += "\n  vtable->destructor: " + std::to_string(reinterpret_cast<uintptr_t>(vtable->destructor));
    } else {
        debug_msg += "\n  vtable->destructor: (vtable is null)";
    }
    LOG_RUNTIME_DEBUG(debug_msg.c_str());
    
    // TRACK OBJECT ALLOCATION
    track_object_allocation(header_ptr, type_id);
    
    // The memory for data_size immediately follows the header.
    // The caller will typically cast (header_ptr + 1) to their specific object type.
    return header_ptr;
}

void Mycelium_Object_retain(MyceliumObjectHeader* obj_header) {
    if (obj_header == NULL) {
        return;
    }
    
    // TRACK OBJECT RETAIN
    track_object_retain(obj_header);
    
    // Use atomic increment for thread safety
    int32_t new_ref_count = Mycelium_Object_atomic_increment(obj_header);
    
    // Check for overflow (extremely unlikely but indicates a serious bug)
    if (new_ref_count <= 0) {
        LOG_RUNTIME_ERROR("Reference count overflow detected - possible memory corruption");
    }
}

void Mycelium_Object_release(MyceliumObjectHeader* obj_header) {
    if (obj_header == NULL) {
        return;
    }
    
    // TRACK OBJECT RELEASE FIRST
    track_object_release(obj_header);
    
    // Use atomic decrement for thread safety
    int32_t new_ref_count = Mycelium_Object_atomic_decrement(obj_header);
    
    // Check for underflow (indicates double-release bug)
    if (new_ref_count < 0) {
        LOG_RUNTIME_ERROR("Reference count underflow detected - possible double-release bug");
        return; // Don't free, this is a critical bug
    }
    
    if (new_ref_count == 0) {
        // RUNTIME DESTRUCTOR DISPATCH (for polymorphic scenarios)
        // Note: For monomorphic code, the compiler calls destructors directly before this
        if (obj_header->vtable != nullptr && obj_header->vtable->destructor != nullptr) {
            // Get pointer to object's data fields (skip the header)
            void* obj_fields_ptr = (void*)((char*)obj_header + sizeof(MyceliumObjectHeader));
            obj_header->vtable->destructor(obj_fields_ptr);
        }
        
        // Free the object (no sentinel values - clean approach)
        free(obj_header);
    }
}

int32_t Mycelium_Object_get_ref_count(MyceliumObjectHeader* obj_header) {
    if (obj_header == NULL) {
        return -1;
    }
    return Mycelium_Object_atomic_load(obj_header);
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

// --- Thread-safe atomic operations for reference counting ---
#ifdef __cplusplus
int32_t Mycelium_Object_atomic_increment(MyceliumObjectHeader* obj_header) {
    if (obj_header == NULL) {
        return -1;
    }
    return obj_header->ref_count.fetch_add(1) + 1;
}

int32_t Mycelium_Object_atomic_decrement(MyceliumObjectHeader* obj_header) {
    if (obj_header == NULL) {
        return -1;
    }
    return obj_header->ref_count.fetch_sub(1) - 1;
}

int32_t Mycelium_Object_atomic_load(MyceliumObjectHeader* obj_header) {
    if (obj_header == NULL) {
        return -1;
    }
    return obj_header->ref_count.load();
}

void Mycelium_Object_atomic_store(MyceliumObjectHeader* obj_header, int32_t value) {
    if (obj_header != NULL) {
        obj_header->ref_count.store(value);
    }
}
#endif
