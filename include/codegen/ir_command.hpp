#pragma once
#include <vector>
#include <variant>
#include <string>
#include <cstdint>
#include <memory>

namespace Mycelium::Scripting::Lang {

// Type-safe operations
enum class Op {
    // Constants
    Const,
    
    // Binary operations
    Add,
    Sub,
    Mul,
    Div,
    
    // Logical operations
    And,
    Or,
    Not,
    
    // Comparison operations
    ICmp,           // Integer comparison (takes comparison predicate)
    
    // Memory operations
    Alloca,
    Load,
    Store,
    GEP,            // GetElementPtr for struct field access
    
    // Control flow
    Label,          // Basic block label
    Br,             // Unconditional branch
    BrCond,         // Conditional branch
    Ret,
    RetVoid,
    
    // Functions
    FunctionBegin,
    FunctionEnd,
    Call
};

// Comparison predicates for ICmp
enum class ICmpPredicate {
    Eq,     // ==
    Ne,     // !=
    Slt,    // <  (signed less than)
    Sle,    // <= (signed less than or equal)
    Sgt,    // >  (signed greater than)
    Sge,    // >= (signed greater than or equal)
    Ult,    // <  (unsigned less than)
    Ule,    // <= (unsigned less than or equal)
    Ugt,    // >  (unsigned greater than)
    Uge     // >= (unsigned greater than or equal)
};

// Forward declaration
struct StructLayout;

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
        Ptr,
        Struct
    } kind;
    
    // For struct types, contains layout information
    std::shared_ptr<StructLayout> struct_layout;
    
    // For pointer types, the pointee type
    std::shared_ptr<IRType> pointee_type;
    
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
    static IRType ptr_to(IRType pointee);
    static IRType struct_(std::shared_ptr<StructLayout> layout);
    
    bool operator==(const IRType& other) const;
    bool operator!=(const IRType& other) const { return !(*this == other); }
    
    // Get size of type in bytes
    size_t size_in_bytes() const;
    
    // Get alignment requirement in bytes
    size_t alignment() const;
    
    // For debugging/printing
    std::string to_string() const;
};

// Struct layout information
struct StructLayout {
    struct Field {
        std::string name;
        IRType type;
        size_t offset;  // Byte offset from struct start
    };
    
    std::string name;  // Struct type name
    std::vector<Field> fields;
    size_t total_size;
    size_t alignment;
    
    // Calculate layout from fields (sets offsets, total_size, alignment)
    void calculate_layout();
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
    
    // Immediate data for constants, names, and operation-specific data
    std::variant<
        std::monostate,    // No data
        int64_t,           // Integer constants
        bool,              // Boolean constants
        double,            // Float constants
        std::string,       // Names (functions, types, labels)
        ICmpPredicate      // Comparison predicates
    > data;
    
    Command() = default;
    Command(Op operation, ValueRef res, std::vector<ValueRef> arguments)
        : op(operation), result(res), args(std::move(arguments)) {}
    
    // Convert command to readable string for debugging
    std::string to_string() const;
};

} // namespace Mycelium::Scripting::Lang