// codegen.hpp - HLIR to LLVM IR Lowering
#pragma once

#include "hlir/hlir.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <unordered_map>
#include <memory>
#include <string>

namespace Fern
{

    /**
     * @brief Lowers HLIR to LLVM IR
     *
     * This class takes a complete HLIR Module and generates LLVM IR.
     * The process is:
     * 1. Declare all types (struct definitions)
     * 2. Declare all functions (signatures)
     * 3. Generate function bodies (instructions)
     */
    class HLIRCodeGen
    {
    private:
        // LLVM core objects
        llvm::LLVMContext &context;
        std::unique_ptr<llvm::Module> module;
        std::unique_ptr<llvm::IRBuilder<>> builder;

        // Type mapping: HLIR TypePtr -> LLVM Type
        std::unordered_map<TypePtr, llvm::Type *> type_map;

        // Type definition mapping: HLIR TypeDefinition -> LLVM StructType
        std::unordered_map<HLIR::TypeDefinition *, llvm::StructType *> struct_map;

        // Function mapping: HLIR Function -> LLVM Function
        std::unordered_map<HLIR::Function *, llvm::Function *> function_map;

        // Value mapping: HLIR Value -> LLVM Value (per function, cleared between functions)
        std::unordered_map<HLIR::Value *, llvm::Value *> value_map;

        // Block mapping: HLIR BasicBlock -> LLVM BasicBlock (per function)
        std::unordered_map<HLIR::BasicBlock *, llvm::BasicBlock *> block_map;

        // Current function being generated
        HLIR::Function *current_hlir_function = nullptr;
        llvm::Function *current_llvm_function = nullptr;

        // Pending phi nodes (need to be resolved after all blocks are generated)
        std::vector<std::pair<llvm::PHINode*, HLIR::PhiInst*>> pending_phis;

    public:
        HLIRCodeGen(llvm::LLVMContext &ctx, const std::string &module_name)
            : context(ctx)
        {
            module = std::make_unique<llvm::Module>(module_name, context);
            builder = std::make_unique<llvm::IRBuilder<>>(context);
        }

        // Main entry point: lower entire HLIR module to LLVM IR
        std::unique_ptr<llvm::Module> lower(HLIR::Module *hlir_module);

        // Get the generated module (transfers ownership)
        std::unique_ptr<llvm::Module> release_module() { return std::move(module); }

    private:
        // === Phase 1: Type Declaration ===
        void declare_types(HLIR::Module *hlir_module);
        llvm::Type *get_or_create_type(TypePtr type);
        llvm::StructType *declare_struct_type(HLIR::TypeDefinition *type_def);

        // === Phase 2: Function Declaration ===
        void declare_functions(HLIR::Module *hlir_module);
        llvm::Function *declare_function(HLIR::Function *hlir_func);
        llvm::FunctionType *get_function_type(HLIR::Function *hlir_func);

        // === Phase 3: Function Body Generation ===
        void generate_function_bodies(HLIR::Module *hlir_module);
        void generate_function_body(HLIR::Function *hlir_func);

        // Basic block generation
        void generate_basic_block(HLIR::BasicBlock *hlir_block);

        // Instruction generation
        void generate_instruction(HLIR::Instruction *inst);

        // Specific instruction handlers
        void gen_const_int(HLIR::ConstIntInst *inst);
        void gen_const_float(HLIR::ConstFloatInst *inst);
        void gen_const_bool(HLIR::ConstBoolInst *inst);
        void gen_const_string(HLIR::ConstStringInst *inst);
        void gen_alloc(HLIR::AllocInst *inst);
        void gen_load(HLIR::LoadInst *inst);
        void gen_store(HLIR::StoreInst *inst);
        void gen_field_addr(HLIR::FieldAddrInst *inst);
        void gen_element_addr(HLIR::ElementAddrInst *inst);
        void gen_binary(HLIR::BinaryInst *inst);
        void gen_unary(HLIR::UnaryInst *inst);
        void gen_cast(HLIR::CastInst *inst);
        void gen_call(HLIR::CallInst *inst);
        void gen_ret(HLIR::RetInst *inst);
        void gen_br(HLIR::BrInst *inst);
        void gen_cond_br(HLIR::CondBrInst *inst);
        void gen_phi(HLIR::PhiInst *inst);

        // Helper: Get LLVM value for HLIR value
        llvm::Value *get_value(HLIR::Value *hlir_value);

        // Helper: Get LLVM basic block for HLIR basic block
        llvm::BasicBlock *get_block(HLIR::BasicBlock *hlir_block);

        // Type helpers
        bool is_signed_int(TypePtr type);
        bool is_unsigned_int(TypePtr type);
        bool is_float(TypePtr type);
    };

} // namespace Fern