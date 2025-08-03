#pragma once

#include <cstdint>
#include <string>

namespace Myre {

// Forward declaration
struct Type;

// Simple IR type representation
struct IRType {
    enum Kind { Void, I1, I8, I16, I32, I64, F32, F64, Ptr, Array, Struct };
    
    Kind kind;
    IRType* element_type;  // For ptr/array
    size_t size;           // For array
    
    IRType(Kind k) : kind(k), element_type(nullptr), size(0) {}
    
    static IRType void_type;
    static IRType i1_type;
    static IRType i8_type;
    static IRType i32_type;
    static IRType i64_type;
    static IRType f32_type;
    static IRType f64_type;
    
    bool is_integer() const {
        return kind == I1 || kind == I8 || kind == I16 || kind == I32 || kind == I64;
    }
    
    bool is_floating() const {
        return kind == F32 || kind == F64;
    }
    
    std::string to_string() const {
        switch (kind) {
            case Void: return "void";
            case I1: return "i1";
            case I8: return "i8";
            case I16: return "i16";
            case I32: return "i32";
            case I64: return "i64";
            case F32: return "f32";
            case F64: return "f64";
            case Ptr: return element_type ? element_type->to_string() + "*" : "ptr";
            case Array: return element_type ? element_type->to_string() + "[" + std::to_string(size) + "]" : "array";
            case Struct: return "struct";
        }
        return "unknown";
    }
};

// Simple value reference - just an ID and type
struct ValueRef {
    int32_t id;
    IRType type;
    
    ValueRef() : id(-1), type(IRType::void_type) {}
    ValueRef(int32_t i, IRType t) : id(i), type(t) {}
    
    bool is_valid() const { return id >= 0; }
    
    static ValueRef invalid() { return ValueRef(); }
    
    std::string to_string() const {
        if (!is_valid()) return "<invalid>";
        return "%" + std::to_string(id);
    }
};

} // namespace Myre