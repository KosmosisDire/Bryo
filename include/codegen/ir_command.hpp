#pragma once
#include <vector>
#include <variant>
#include <string>
#include <cstdint>

namespace codegen {

// Type-safe operations
enum class Op {
    // Constants
    Const,
    
    // Binary operations
    Add,
    Sub,
    Mul,
    Div,
    
    // Memory operations
    Alloca,
    Load,
    Store,
    
    // Control flow
    Ret,
    RetVoid,
    
    // Functions
    FunctionBegin,
    FunctionEnd,
    Call
};

// Simple type representation (opaque pointers)
struct IRType {
    enum Kind : uint8_t {
        Void, 
        I32,
        I64,
        I8,
        I16,
        Bool,
        F32,
        F64,
        Ptr
    } kind;
    
    IRType(Kind k = Kind::Void) : kind(k) {}
    
    // Factory methods
    static IRType i32() { return {Kind::I32}; }
    static IRType i64() { return {Kind::I64}; }
    static IRType i8() { return {Kind::I8}; }
    static IRType i16() { return {Kind::I16}; }
    static IRType bool_() { return {Kind::Bool}; }
    static IRType f32() { return {Kind::F32}; }
    static IRType f64() { return {Kind::F64}; }
    static IRType void_() { return {Kind::Void}; }
    static IRType ptr() { return {Kind::Ptr}; }
    
    bool operator==(const IRType& other) const { return kind == other.kind; }
    bool operator!=(const IRType& other) const { return kind != other.kind; }
    
    // For debugging/printing
    const char* to_string() const {
        switch (kind) {
            case Kind::Void: return "void";
            case Kind::I32: return "i32";
            case Kind::I64: return "i64";
            case Kind::I8: return "i8";
            case Kind::I16: return "i16";
            case Kind::Bool: return "i1";
            case Kind::F32: return "f32";
            case Kind::F64: return "f64";
            case Kind::Ptr: return "ptr";
            default: return "unknown";
        }
    }
};

// Lightweight value reference
struct ValueRef {
    int id;
    IRType type;
    
    ValueRef() : id(-1), type(IRType::Kind::Void) {}
    ValueRef(int i, IRType t) : id(i), type(t) {}
    
    bool is_valid() const { return id >= 0; }
    static ValueRef invalid() { return ValueRef(); }
};

// Command structure
struct Command {
    Op op;
    ValueRef result;
    std::vector<ValueRef> args;
    
    // Immediate data for constants and names
    std::variant<
        std::monostate,    // No data
        int64_t,           // Integer constants
        bool,              // Boolean constants
        double,            // Float constants
        std::string        // Names (functions, types)
    > data;
    
    Command() = default;
    Command(Op operation, ValueRef res, std::vector<ValueRef> arguments)
        : op(operation), result(res), args(std::move(arguments)) {}
};

} // namespace codegen