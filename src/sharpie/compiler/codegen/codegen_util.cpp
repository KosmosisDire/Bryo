#include "sharpie/compiler/codegen/codegen.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <stdexcept>

namespace Mycelium::Scripting::Lang::CodeGen {

llvm::Type* get_llvm_type(CodeGenContext& ctx, std::shared_ptr<TypeNameNode> type_node) {
    if (!type_node) {
        log_compiler_error("TypeNameNode is null when trying to get LLVM type.");
    }

    // Handle IdentifierNode for primitive/class names
    if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&type_node->name_segment)) {
        if (auto identNode = *identNode_variant) {
            return get_llvm_type_from_string(ctx, identNode->name, type_node->location);
        }
    }
    
    // TODO: Handle QualifiedNameNode for namespaced types
    if (auto qualifiedNode_variant = std::get_if<std::shared_ptr<QualifiedNameNode>>(&type_node->name_segment)) {
        if (auto qualifiedNode = *qualifiedNode_variant) {
            if (qualifiedNode->right)
            {
                // for now just take the rightmost identifier
                return get_llvm_type_from_string(ctx, qualifiedNode->right->name, qualifiedNode->right->location);
            }
        }
    }
    
    log_compiler_error("Unsupported TypeNameNode structure for LLVM type conversion.", type_node->location);
}

llvm::Type* get_llvm_type_from_string(CodeGenContext& ctx, const std::string& type_name, std::optional<SourceLocation> loc) {
    if (type_name == "int") return llvm::Type::getInt32Ty(ctx.llvm_context);
    if (type_name == "long") return llvm::Type::getInt64Ty(ctx.llvm_context);
    if (type_name == "bool") return llvm::Type::getInt1Ty(ctx.llvm_context);
    if (type_name == "char") return llvm::Type::getInt8Ty(ctx.llvm_context);
    if (type_name == "float") return llvm::Type::getFloatTy(ctx.llvm_context);
    if (type_name == "double") return llvm::Type::getDoubleTy(ctx.llvm_context);
    if (type_name == "void") return llvm::Type::getVoidTy(ctx.llvm_context);

    // For string and any other class type, we use an opaque pointer
    if (type_name == "string" || ctx.symbol_table.find_class(type_name)) {
        return llvm::PointerType::get(ctx.llvm_context, 0);
    }

    log_compiler_error("Unknown type name: '" + type_name + "' encountered during LLVM type lookup.", loc);
}

llvm::AllocaInst* create_entry_block_alloca(CodeGenContext& ctx, const std::string& var_name, llvm::Type* type) {
    if (!ctx.current_function) {
        log_compiler_error("Cannot create alloca: current_function is null.");
    }
    // Create a temporary builder that is guaranteed to insert at the top of the entry block
    llvm::IRBuilder<> TmpB(&ctx.current_function->getEntryBlock(), ctx.current_function->getEntryBlock().begin());
    return TmpB.CreateAlloca(type, nullptr, var_name.c_str());
}

llvm::Value* get_header_ptr_from_fields_ptr(CodeGenContext& ctx, llvm::Value* fieldsPtr, llvm::StructType* fieldsLLVMType) {
    if (fieldsPtr->getType() == llvm::PointerType::get(ctx.llvm_context, 0)) {
        // Correctly get the size of the header struct
        llvm::StructType* headerType = llvm::StructType::getTypeByName(ctx.llvm_context, "struct.MyceliumObjectHeader");
        if (!headerType) {
            log_compiler_error("MyceliumObjectHeader LLVM type not found.");
        }
        uint64_t header_size = ctx.llvm_module.getDataLayout().getTypeAllocSize(headerType);

        // GEP on an i8* to do byte-level pointer arithmetic
        llvm::Value* offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvm_context), -static_cast<int64_t>(header_size));
        return ctx.builder.CreateGEP(llvm::Type::getInt8Ty(ctx.llvm_context), fieldsPtr, offset, "header.ptr.from.fields");
    }
    log_compiler_error("get_header_ptr_from_fields_ptr expects an opaque pointer.");
}

llvm::Value* get_fields_ptr_from_header_ptr(CodeGenContext& ctx, llvm::Value* headerPtr, llvm::StructType* fieldsLLVMType) {
    if (headerPtr->getType() == llvm::PointerType::get(ctx.llvm_context, 0)) {
        // Correctly get the size of the header struct
        llvm::StructType* headerType = llvm::StructType::getTypeByName(ctx.llvm_context, "struct.MyceliumObjectHeader");
        if (!headerType) {
            log_compiler_error("MyceliumObjectHeader LLVM type not found.");
        }
        uint64_t header_size = ctx.llvm_module.getDataLayout().getTypeAllocSize(headerType);
        
        // GEP on an i8* to do byte-level pointer arithmetic
        llvm::Value* offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvm_context), header_size);
        return ctx.builder.CreateGEP(llvm::Type::getInt8Ty(ctx.llvm_context), headerPtr, offset, "fields.ptr.from.header");
    }
    log_compiler_error("get_fields_ptr_from_header_ptr expects an opaque pointer.");
}

void create_arc_retain(CodeGenContext& ctx, llvm::Value* object_header_ptr) {
    if (!object_header_ptr) return;
    llvm::Function* retain_func = ctx.llvm_module.getFunction("Mycelium_Object_retain");
    if (!retain_func) {
        log_compiler_error("Runtime function Mycelium_Object_retain not found.");
    }
    ctx.builder.CreateCall(retain_func, {object_header_ptr});
}

void create_arc_release(CodeGenContext& ctx, llvm::Value* object_header_ptr) {
    if (!object_header_ptr) return;
    llvm::Function* release_func = ctx.llvm_module.getFunction("Mycelium_Object_release");
    if (!release_func) {
        log_compiler_error("Runtime function Mycelium_Object_release not found.");
    }
    ctx.builder.CreateCall(release_func, {object_header_ptr});
}

llvm::Value* create_string_from_literal(CodeGenContext& ctx, const std::string& str) {
    llvm::Function* new_str_func = ctx.llvm_module.getFunction("Mycelium_String_new_from_literal");
    if (!new_str_func) {
        log_compiler_error("Runtime function Mycelium_String_new_from_literal not found.");
    }

    // Create a global constant for the string literal
    llvm::Constant* str_const = llvm::ConstantDataArray::getString(ctx.llvm_context, str, true);
    auto* global_var = new llvm::GlobalVariable(ctx.llvm_module, str_const->getType(), true, llvm::GlobalValue::PrivateLinkage, str_const, ".str");
    global_var->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    global_var->setAlignment(llvm::MaybeAlign(1));

    // Get a pointer to the start of the string
    llvm::Value* char_ptr = ctx.builder.CreateBitCast(global_var, llvm::PointerType::get(llvm::Type::getInt8Ty(ctx.llvm_context), 0));
    llvm::Value* len_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvm_context), str.length());

    return ctx.builder.CreateCall(new_str_func, {char_ptr, len_val}, "new_mycelium_str");
}

// Helper function to get the Mycelium string pointer type (used for string operations)
llvm::Type* get_mycelium_string_ptr_ty(CodeGenContext& ctx) {
    return llvm::PointerType::get(ctx.llvm_context, 0);
}

// Helper function to convert LLVM type to string for error messages
std::string llvm_type_to_string(llvm::Type* type) {
    if (!type) return "null";
    
    std::string str;
    llvm::raw_string_ostream rso(str);
    type->print(rso);
    return rso.str();
}

[[noreturn]] void log_compiler_error(const std::string& message, std::optional<SourceLocation> loc) {
    std::stringstream ss;
    ss << "Compiler Error";
    if (loc && loc->fileName != "unknown") {
        ss << " at " << loc->to_string();
    }
    ss << ": " << message;
    throw std::runtime_error(ss.str());
}

} // namespace Mycelium::Scripting::Lang::CodeGen