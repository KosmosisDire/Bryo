#pragma once

#include "codegen_context.hpp"
#include "sharpie/ast/ast_types.hpp"
#include "sharpie/ast/ast_location.hpp"

#include <string>
#include <memory>
#include <optional>

namespace llvm {
    class Type;
    class Value;
    class AllocaInst;
    class Function;
    class StructType;
}

namespace Mycelium::Scripting::Lang::CodeGen {

/**
 * @brief Converts a TypeNameNode from the AST into its corresponding LLVM Type.
 * @param ctx The code generation context.
 * @param type_node The AST node representing the type.
 * @return The corresponding llvm::Type*, or nullptr if the type is unknown.
 */
llvm::Type* get_llvm_type(CodeGenContext& ctx, std::shared_ptr<TypeNameNode> type_node);

/**
 * @brief Converts a string representation of a type into its corresponding LLVM Type.
 * @param ctx The code generation context.
 * @param type_name The string name of the type (e.g., "int", "MyClass").
 * @param loc The source location for error reporting.
 * @return The corresponding llvm::Type*, or nullptr if the type is unknown.
 */
llvm::Type* get_llvm_type_from_string(CodeGenContext& ctx, const std::string& type_name, std::optional<SourceLocation> loc = std::nullopt);

/**
 * @brief Creates an 'alloca' instruction in the entry block of the current function.
 *
 * This is the standard way to declare a mutable local variable on the stack.
 * All alloca instructions should be in the entry block for optimization.
 *
 * @param ctx The code generation context, containing the current function.
 * @param var_name The name of the variable, for debugging/IR readability.
 * @param type The LLVM type to allocate space for.
 * @return A pointer to the created llvm::AllocaInst.
 */
llvm::AllocaInst* create_entry_block_alloca(CodeGenContext& ctx, const std::string& var_name, llvm::Type* type);

/**
 * @brief Given a pointer to an object's fields, calculates the pointer to its header.
 * @param ctx The code generation context.
 * @param fieldsPtr A pointer to the start of the object's data fields.
 * @param fieldsLLVMType The LLVM struct type of the fields.
 * @return An llvm::Value* pointing to the MyceliumObjectHeader.
 */
llvm::Value* get_header_ptr_from_fields_ptr(CodeGenContext& ctx, llvm::Value* fieldsPtr, llvm::StructType* fieldsLLVMType);

/**
 * @brief Given a pointer to an object's header, calculates the pointer to its fields.
 * @param ctx The code generation context.
 * @param headerPtr A pointer to the MyceliumObjectHeader.
 * @param fieldsLLVMType The LLVM struct type of the fields.
 * @return An llvm::Value* pointing to the start of the object's data fields.
 */
llvm::Value* get_fields_ptr_from_header_ptr(CodeGenContext& ctx, llvm::Value* headerPtr, llvm::StructType* fieldsLLVMType);

/**
 * @brief Creates a call to the Mycelium_Object_retain runtime function for ARC.
 * @param ctx The code generation context.
 * @param object_header_ptr A pointer to the object's MyceliumObjectHeader.
 */
void create_arc_retain(CodeGenContext& ctx, llvm::Value* object_header_ptr);

/**
 * @brief Creates a call to the Mycelium_Object_release runtime function for ARC.
 * @param ctx The code generation context.
 * @param object_header_ptr A pointer to the object's MyceliumObjectHeader.
 */
void create_arc_release(CodeGenContext& ctx, llvm::Value* object_header_ptr);

/**
 * @brief Creates a MyceliumString from a C++ string literal by calling the runtime.
 * @param ctx The code generation context.
 * @param str The string literal value.
 * @return An llvm::Value* representing the new MyceliumString*.
 */
llvm::Value* create_string_from_literal(CodeGenContext& ctx, const std::string& str);

/**
 * @brief Helper function to get the Mycelium string pointer type.
 * @param ctx The code generation context.
 * @return The LLVM type for Mycelium string pointers.
 */
llvm::Type* get_mycelium_string_ptr_ty(CodeGenContext& ctx);

/**
 * @brief Converts an LLVM type to a string representation for error messages.
 * @param type The LLVM type to convert.
 * @return A string representation of the type.
 */
std::string llvm_type_to_string(llvm::Type* type);

/**
 * @brief Logs a compiler error and throws a runtime_error to halt compilation.
 * @param message The error message.
 * @param loc The source location of the error.
 */
[[noreturn]] void log_compiler_error(const std::string& message, std::optional<SourceLocation> loc = std::nullopt);

} // namespace Mycelium::Scripting::Lang::CodeGen