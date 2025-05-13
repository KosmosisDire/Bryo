#include "script_compiler.hpp"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/CFG.h" // For llvm::pred_begin, llvm::pred_end

// For JIT and AOT
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h" // For JIT (though ExecutionEngine uses GenericValue directly)
#include "llvm/ExecutionEngine/MCJIT.h"      // For MCJIT if used explicitly (EngineBuilder handles it)
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/InstCombine/InstCombine.h" // Example pass
#include "llvm/Transforms/Scalar.h"                 // Example pass (like Reassociate)
#include "llvm/Transforms/Scalar/GVN.h"             // Example pass
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/CodeGen.h"

#include <iostream>
#include <sstream>
#include <fstream> // For saving object file in AOT (though raw_fd_ostream is used)

namespace Mycelium::Scripting::Lang
{

// --- ScriptCompiler Implementation ---

ScriptCompiler::ScriptCompiler() {
    m_context = std::make_unique<llvm::LLVMContext>();
    // Module and Builder are initialized in compile_ast()
}

ScriptCompiler::~ScriptCompiler() = default;

void ScriptCompiler::compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string& module_name) {
    if (!m_context) { // Should always be true due to constructor
        m_context = std::make_unique<llvm::LLVMContext>();
    }
    m_module = std::make_unique<llvm::Module>(module_name, *m_context);
    m_builder = std::make_unique<llvm::IRBuilder<>>(*m_context);
    m_named_values.clear();
    m_current_function = nullptr;

    if (!ast_root) {
        log_error("AST root is null. Cannot compile.");
    }
    visit(ast_root);

    if (llvm::verifyModule(*m_module, &llvm::errs())) {
        log_error("LLVM Module verification failed. Dumping potentially corrupt IR.");
        throw std::runtime_error("LLVM module verification failed.");
    }
}

std::string ScriptCompiler::get_ir_string() const {
    if (!m_module) return "[No LLVM Module Initialized]";
    std::string irString;
    llvm::raw_string_ostream ostream(irString);
    m_module->print(ostream, nullptr);
    ostream.flush();
    return irString;
}

void ScriptCompiler::dump_ir() const {
    if (!m_module) {
        llvm::errs() << "[No LLVM Module Initialized to dump]\n";
        return;
    }
    m_module->print(llvm::errs(), nullptr);
}

std::unique_ptr<llvm::Module> ScriptCompiler::take_module() {
    if (!m_module) {
        log_error("Attempted to take a null module. Compile AST first.");
    }
    return std::move(m_module);
}


[[noreturn]] void ScriptCompiler::log_error(const std::string& message, std::optional<SourceLocation> loc) {
    std::stringstream ss;
    ss << "Script Compiler Error";
    if (loc) {
        ss << " " << loc->toString();
    }
    ss << ": " << message;
    llvm::errs() << ss.str() << "\n";
    throw std::runtime_error(ss.str());
}

std::string ScriptCompiler::llvm_type_to_string(llvm::Type* type) const {
    if (!type) return "<null llvm type>";
    std::string typeStr;
    llvm::raw_string_ostream rso(typeStr);
    type->print(rso);
    return rso.str();
}

// --- JIT Execution ---
bool jit_initialized = false;
void ScriptCompiler::initialize_jit_engine_dependencies() {
    if (!jit_initialized) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser(); // Though not strictly needed for basic JIT runFunction
        jit_initialized = true;
    }
}

llvm::GenericValue ScriptCompiler::jit_execute_function(
    const std::string& function_name,
    const std::vector<llvm::GenericValue>& args) {
    initialize_jit_engine_dependencies();

    if (!m_module) {
        log_error("No LLVM module available for JIT. Compile AST first.");
    }

    std::string errStr;
    // EngineBuilder takes ownership of the module
    llvm::ExecutionEngine *ee = llvm::EngineBuilder(take_module())
                                      .setErrorStr(&errStr)
                                      .setEngineKind(llvm::EngineKind::JIT) // Or ::MCJIT for more features
                                      .create();

    if (!ee) {
        log_error("Failed to construct ExecutionEngine: " + errStr);
    }

    // Find the function. Note: name might be mangled or adjusted (e.g., to "main")
    llvm::Function *funcToRun = ee->FindFunctionNamed(function_name);
    if (!funcToRun) {
        delete ee; // Important to clean up
        log_error("JIT: Function '" + function_name + "' not found in module.");
    }

    llvm::GenericValue result;
    try {
        result = ee->runFunction(funcToRun, args);
    } catch (const std::exception& e) {
        delete ee;
        log_error("JIT: Exception during runFunction for '" + function_name + "': " + e.what());
    } catch (...) {
        delete ee;
        log_error("JIT: Unknown exception during runFunction for '" + function_name + "'.");
    }
    
    delete ee; // Clean up the execution engine
    return result;
}

// --- AOT Compilation to Object File ---
bool aot_initialized = false;
void ScriptCompiler::initialize_aot_engine_dependencies() {
    if (!aot_initialized) {
        llvm::InitializeAllTargets(); // More comprehensive for AOT to various targets
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmPrinters();
        llvm::InitializeAllAsmParsers();
        aot_initialized = true;
    }
}

void ScriptCompiler::compile_to_object_file(const std::string& output_filename) {
    initialize_aot_engine_dependencies();

    if (!m_module) {
        log_error("No LLVM module available for AOT compilation. Compile AST first.");
    }
    
    // It's generally safer to work on a copy or re-generate if the module is modified by passes.
    // However, for direct emission after `compile_ast`, m_module should be fine.

    auto target_triple_str = llvm::sys::getDefaultTargetTriple();
    m_module->setTargetTriple(target_triple_str);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple_str, error);
    if (!target) {
        log_error("Error looking up target '" + target_triple_str + "': " + error);
    }

    auto cpu = "generic"; // Or llvm::sys::getHostCPUName() for current CPU
    auto features = "";   // Specific CPU features string

    llvm::TargetOptions opt;
    // llvm::Reloc::Model rm = llvm::Reloc::PIC_; // Position Independent Code, common for shared libs
    auto rm = std::optional<llvm::Reloc::Model>();
    llvm::TargetMachine* target_machine = target->createTargetMachine(target_triple_str, cpu, features, opt, rm);

    if (!target_machine) {
        log_error("Could not create TargetMachine for triple '" + target_triple_str + "'.");
    }

    m_module->setDataLayout(target_machine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(output_filename, ec, llvm::sys::fs::OF_None); // OF_None should handle binary output correctly

    if (ec) {
        log_error("Could not open file '" + output_filename + "': " + ec.message());
    }

    llvm::legacy::PassManager pass_manager;
    llvm::CodeGenFileType file_type = llvm::CodeGenFileType::ObjectFile;

    // Example: Add some optimization passes (optional, but good for generated code)
    // pass_manager.add(llvm::createInstructionCombiningPass());
    // pass_manager.add(llvm::createReassociatePass());
    // pass_manager.add(llvm::createGVNPass());
    // pass_manager.add(llvm::createCFGSimplificationPass()); // In "llvm/Transforms/Scalar.h"

    if (target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, file_type)) {
        log_error("TargetMachine can't emit a file of type 'ObjectFile'.");
    }

    pass_manager.run(*m_module); // Run passes, including emission
    dest.flush(); // Ensure all data is written
    // dest.close(); // raw_fd_ostream closes on destruction

    // llvm::outs() << "Object file '" << output_filename << "' emitted successfully.\n";
    // The module is effectively "consumed" by this process in terms of its state
    // if passes modified it. If you need to JIT *after* this, you might need to re-compile AST
    // or clone the module before AOT.
}


// --- Visitor Dispatchers ---

llvm::Value* ScriptCompiler::visit(std::shared_ptr<AstNode> node) {
    if (!node) {
        log_error("Attempted to visit a null AST node.");
    }

    if (auto specificNode = std::dynamic_pointer_cast<CompilationUnitNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<ClassDeclarationNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<BlockStatementNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<LocalVariableDeclarationStatementNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<ExpressionStatementNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<IfStatementNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<ReturnStatementNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<LiteralExpressionNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<IdentifierExpressionNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<BinaryExpressionNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<AssignmentExpressionNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<UnaryExpressionNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<MethodCallExpressionNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node)) return visit(specificNode);
    if (auto specificNode = std::dynamic_pointer_cast<ThisExpressionNode>(node)) return visit(specificNode);
    
    log_error("Encountered an AST node type for which a specific 'visit' method is not implemented.", node->location);
    return nullptr;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<StatementNode> node) {
    return visit(std::static_pointer_cast<AstNode>(node));
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<ExpressionNode> node) {
    return visit(std::static_pointer_cast<AstNode>(node));
}


// --- Top-Level Visitors ---

llvm::Value* ScriptCompiler::visit(std::shared_ptr<CompilationUnitNode> node) {
    for (const auto& member : node->members) {
        if (auto classDecl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            visit(classDecl);
        } else {
            log_error("Unsupported top-level member. Expected ClassDeclarationNode.", member->location);
        }
    }
    return nullptr;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<ClassDeclarationNode> node) {
    std::string class_name = node->name;
    for (const auto& member : node->members) {
        if (auto methodDecl = std::dynamic_pointer_cast<MethodDeclarationNode>(member)) {
            bool is_static = false;
            for (ModifierKind mod : methodDecl->modifiers) {
                if (mod == ModifierKind::Static) {
                    is_static = true;
                    break;
                }
            }
            if (!is_static && methodDecl->name != class_name) { // Allow constructors which aren't static
                // log_error("Skipping non-static method (not supported in this example): " + class_name + "." + methodDecl->name, methodDecl->location);
                continue; 
            }
            visit_method_declaration(methodDecl, class_name);
        }
    }
    return nullptr;
}

// --- Class Member Visitors ---

llvm::Function* ScriptCompiler::visit_method_declaration(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name) {
    m_named_values.clear(); 

    if (!node->type.has_value()) {
        log_error("Method '" + class_name + "." + node->name + "' lacks a return type AST node.", node->location);
    }
    std::shared_ptr<TypeNameNode> return_type_name_node = node->type.value();
    if (!return_type_name_node) {
        log_error("Method '" + class_name + "." + node->name + "' has a null TypeNameNode for its return type.", node->location);
    }
    llvm::Type* return_type = get_llvm_type(return_type_name_node);
    
    std::vector<llvm::Type*> param_types;
    // Parameter handling would go here

    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
    
    std::string func_name = class_name + "." + node->name;
    // Special handling for "Program.Main" to become "main" for easy linking/JIT
    if (func_name == "Program.Main") {
        func_name = "main";
    }
    
    llvm::Function* function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, m_module.get());
    m_current_function = function;

    // Parameter argument setup would go here if params exist

    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(*m_context, "entry", function);
    m_builder->SetInsertPoint(entry_block);

    if (node->body) {
        visit(node->body.value()); 
        if (m_builder->GetInsertBlock()->getTerminator() == nullptr) {
            if (return_type->isVoidTy()) {
                m_builder->CreateRetVoid();
            } // Else, verifier will catch missing return for non-void.
        }
    } else {
        log_error("Method '" + func_name + "' has no body.", node->location);
    }

    if (llvm::verifyFunction(*function, &llvm::errs())) {
        std::string f_name_for_err = function->getName().str(); // Get actual name from function obj
        log_error("LLVM Function verification failed for: " + f_name_for_err, node->location);
        // function->print(llvm::errs()); // Already printed by verifyFunction
        // m_module->print(llvm::errs(), nullptr);
        throw std::runtime_error("LLVM function '" + f_name_for_err + "' verification failed.");
    }
    
    m_current_function = nullptr; 
    return function;
}

// --- Statement Visitors ---

llvm::Value* ScriptCompiler::visit(std::shared_ptr<BlockStatementNode> node) {
    llvm::Value* last_val = nullptr;
    for (const auto& stmt : node->statements) {
        if (m_builder->GetInsertBlock()->getTerminator()) {
            break; 
        }
        last_val = visit(stmt);
    }
    return last_val;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<LocalVariableDeclarationStatementNode> node) {
    llvm::Type* var_type = get_llvm_type(node->type);
    for (const auto& declarator : node->declarators) {
        if (m_named_values.count(declarator->name)) {
            log_error("Variable '" + declarator->name + "' redeclared.", declarator->location);
        }
        llvm::AllocaInst* alloca = create_entry_block_alloca(m_current_function, declarator->name, var_type);
        m_named_values[declarator->name] = alloca;
        if (declarator->initializer) {
            llvm::Value* init_val = visit(declarator->initializer.value());
            if (init_val->getType() != var_type) {
                if (var_type->isIntegerTy() && init_val->getType()->isIntegerTy()) {
                    if (var_type->getIntegerBitWidth() > init_val->getType()->getIntegerBitWidth())
                        init_val = m_builder->CreateSExt(init_val, var_type, "init.sext");
                    else if (var_type->getIntegerBitWidth() < init_val->getType()->getIntegerBitWidth())
                        init_val = m_builder->CreateTrunc(init_val, var_type, "init.trunc");
                } else {
                    log_error("Type mismatch for initializer of '" + declarator->name + "'. Expected " +
                             llvm_type_to_string(var_type) + ", got " + llvm_type_to_string(init_val->getType()),
                             declarator->initializer.value()->location);
                }
            }
            m_builder->CreateStore(init_val, alloca);
        }
    }
    return nullptr;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<ExpressionStatementNode> node) {
    return visit(node->expression);
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<IfStatementNode> node) {
    llvm::Value* condition_val = visit(node->condition);
    if (!condition_val) {
         log_error("Condition expression for if-statement failed to generate LLVM Value.", node->condition->location);
    }
    if (condition_val->getType()->isIntegerTy() && condition_val->getType() != llvm::Type::getInt1Ty(*m_context)) {
        condition_val = m_builder->CreateICmpNE(condition_val, llvm::ConstantInt::get(condition_val->getType(), 0, false), "cond.tobool");
    } else if (condition_val->getType() != llvm::Type::getInt1Ty(*m_context)) {
        log_error("If condition must be boolean or integer. Got: " + llvm_type_to_string(condition_val->getType()), node->condition->location);
    }

    llvm::Function* func = m_builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*m_context, "then", func);
    llvm::BasicBlock* else_bb = node->elseStatement ? llvm::BasicBlock::Create(*m_context, "else", func) : nullptr;
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*m_context, "ifcont", func);

    bool then_path_terminated, else_path_terminated = true;

    if (node->elseStatement) {
        m_builder->CreateCondBr(condition_val, then_bb, else_bb);
    } else {
        m_builder->CreateCondBr(condition_val, then_bb, merge_bb);
    }

    m_builder->SetInsertPoint(then_bb);
    visit(node->thenStatement);
    then_path_terminated = (then_bb->getTerminator() != nullptr);
    if (!then_path_terminated) m_builder->CreateBr(merge_bb);

    if (node->elseStatement) {
        m_builder->SetInsertPoint(else_bb);
        visit(node->elseStatement.value());
        else_path_terminated = (else_bb->getTerminator() != nullptr);
        if (!else_path_terminated) m_builder->CreateBr(merge_bb);
    }

    if (llvm::pred_begin(merge_bb) != llvm::pred_end(merge_bb)) {
        m_builder->SetInsertPoint(merge_bb);
    } // Else, merge_bb is dead, LLVM DCE will handle.
    return nullptr;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<ReturnStatementNode> node) {
    if (node->expression) {
        llvm::Value* ret_val = visit(node->expression.value());
        llvm::Type* expected_return_type = m_current_function->getReturnType();
        if (ret_val->getType() != expected_return_type) {
            if (expected_return_type->isIntegerTy() && ret_val->getType()->isIntegerTy()) {
                if (expected_return_type->getIntegerBitWidth() > ret_val->getType()->getIntegerBitWidth())
                    ret_val = m_builder->CreateSExt(ret_val, expected_return_type, "ret.sext");
                else if (expected_return_type->getIntegerBitWidth() < ret_val->getType()->getIntegerBitWidth())
                    ret_val = m_builder->CreateTrunc(ret_val, expected_return_type, "ret.trunc");
            } else {
                 log_error("Return type mismatch. Function '" + std::string(m_current_function->getName()) + 
                          "' expects " + llvm_type_to_string(expected_return_type) + ", but got " + llvm_type_to_string(ret_val->getType()), 
                          node->location);
            }
        }
        m_builder->CreateRet(ret_val);
    } else {
        if (!m_current_function->getReturnType()->isVoidTy()) {
            log_error("Non-void function '" + std::string(m_current_function->getName()) + "' must return a value.", node->location);
        }
        m_builder->CreateRetVoid();
    }
    return nullptr;
}

// --- Expression Visitors ---

llvm::Value* ScriptCompiler::visit(std::shared_ptr<LiteralExpressionNode> node) {
    switch (node->kind) {
        case LiteralKind::Integer:
            try {
                long long val = std::stoll(node->value); 
                return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), val, true);
            } catch (const std::exception& e) {
                log_error("Invalid integer literal '" + node->value + "': " + e.what(), node->location);
            }
        case LiteralKind::Boolean:
            if (node->value == "true") return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*m_context), 1, false);
            if (node->value == "false") return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*m_context), 0, false);
            log_error("Invalid boolean literal: " + node->value, node->location);
        default:
            log_error("Unsupported literal kind: " + Mycelium::Scripting::Lang::to_string(node->kind), node->location);
    }
    llvm_unreachable("Literal visitor error.");
    return nullptr;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<IdentifierExpressionNode> node) {
    auto it = m_named_values.find(node->name);
    if (it == m_named_values.end()) {
        log_error("Undefined variable: " + node->name, node->location);
    }
    return m_builder->CreateLoad(it->second->getAllocatedType(), it->second, node->name.c_str());
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<BinaryExpressionNode> node) {
    llvm::Value* L = visit(node->left);
    llvm::Value* R = visit(node->right);
    if (L->getType() != R->getType()) {
        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
            unsigned LBits = L->getType()->getIntegerBitWidth();
            unsigned RBits = R->getType()->getIntegerBitWidth();
            if (LBits < RBits) L = m_builder->CreateSExt(L, R->getType(), "lhs.sext");
            else if (RBits < LBits) R = m_builder->CreateSExt(R, L->getType(), "rhs.sext");
        } else {
             log_error("Type mismatch in binary op: LHS " + llvm_type_to_string(L->getType()) + 
                      ", RHS " + llvm_type_to_string(R->getType()), node->location);
        }
    }
    switch (node->op) {
        case BinaryOperatorKind::Add:      return m_builder->CreateAdd(L, R, "addtmp");
        case BinaryOperatorKind::Subtract: return m_builder->CreateSub(L, R, "subtmp");
        case BinaryOperatorKind::Multiply: return m_builder->CreateMul(L, R, "multmp");
        case BinaryOperatorKind::Divide:   return m_builder->CreateSDiv(L, R, "divtmp");
        case BinaryOperatorKind::Modulo:   return m_builder->CreateSRem(L, R, "remtmp");
        case BinaryOperatorKind::Equals:   return m_builder->CreateICmpEQ(L, R, "eqtmp");
        case BinaryOperatorKind::NotEquals:return m_builder->CreateICmpNE(L, R, "netmp");
        case BinaryOperatorKind::LessThan: return m_builder->CreateICmpSLT(L, R, "lttmp");
        case BinaryOperatorKind::GreaterThan:return m_builder->CreateICmpSGT(L, R, "gttmp");
        case BinaryOperatorKind::LessThanOrEqual: return m_builder->CreateICmpSLE(L, R, "letmp");
        case BinaryOperatorKind::GreaterThanOrEqual:return m_builder->CreateICmpSGE(L, R, "getmp");
        default: log_error("Unsupported binary op: " + Mycelium::Scripting::Lang::to_string(node->op), node->location);
    }
    llvm_unreachable("Binary op visitor error.");
    return nullptr;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<AssignmentExpressionNode> node) {
    auto id_target = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target);
    if (!id_target) {
        log_error("Assignment target must be a variable.", node->target->location);
    }
    auto it = m_named_values.find(id_target->name);
    if (it == m_named_values.end()) {
        log_error("Assigning to undeclared variable: " + id_target->name, id_target->location);
    }
    llvm::AllocaInst* ptr_to_store = it->second;
    llvm::Value* val_to_store = visit(node->source);
    if (val_to_store->getType() != ptr_to_store->getAllocatedType()) {
         if (ptr_to_store->getAllocatedType()->isIntegerTy() && val_to_store->getType()->isIntegerTy()) {
            llvm::Type* expected_type = ptr_to_store->getAllocatedType();
            if (expected_type->getIntegerBitWidth() > val_to_store->getType()->getIntegerBitWidth())
                val_to_store = m_builder->CreateSExt(val_to_store, expected_type, "assign.sext");
            else if (expected_type->getIntegerBitWidth() < val_to_store->getType()->getIntegerBitWidth())
                val_to_store = m_builder->CreateTrunc(val_to_store, expected_type, "assign.trunc");
        } else {
            log_error("Type mismatch in assignment to '" + id_target->name + "'. Expected " + 
                     llvm_type_to_string(ptr_to_store->getAllocatedType()) + ", got " + llvm_type_to_string(val_to_store->getType()), 
                     node->location);
        }
    }
    m_builder->CreateStore(val_to_store, ptr_to_store);
    return val_to_store;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<UnaryExpressionNode> node) {
    llvm::Value* operand = visit(node->operand);
    switch (node->op) {
        case UnaryOperatorKind::LogicalNot:
            if (operand->getType() != llvm::Type::getInt1Ty(*m_context)) {
                 log_error("LogicalNot (!) expects boolean (i1). Got: " + llvm_type_to_string(operand->getType()), node->operand->location);
            }
            return m_builder->CreateICmpEQ(operand, llvm::ConstantInt::getFalse(*m_context), "nottmp");
        case UnaryOperatorKind::UnaryMinus:
            if (!operand->getType()->isIntegerTy() && !operand->getType()->isFloatingPointTy()) { // Add FP if supported
                 log_error("UnaryMinus (-) expects numeric. Got: " + llvm_type_to_string(operand->getType()), node->operand->location);
            }
            if (operand->getType()->isIntegerTy()) return m_builder->CreateNeg(operand, "negtmp");
            // if (operand->getType()->isFloatingPointTy()) return m_builder->CreateFNeg(operand, "fnegtmp");
            break;
        default: log_error("Unsupported unary op: " + Mycelium::Scripting::Lang::to_string(node->op), node->location);
    }
    llvm_unreachable("Unary op visitor error.");
    return nullptr;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<MethodCallExpressionNode> node) {
    std::string func_name_str; // Determine from node->target
    // This part needs robust implementation based on how methods are identified (static, instance)
    if (auto idTarget = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target)) {
        func_name_str = idTarget->name; // Global/static assumed
    } else if (auto memberAccess = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target)) {
         if (auto classId = std::dynamic_pointer_cast<IdentifierExpressionNode>(memberAccess->target)) {
            func_name_str = classId->name + "." + memberAccess->memberName; 
        } else {
            log_error("Instance method calls not yet supported for member access target.", memberAccess->target->location);
        }
    } else {
        log_error("Unsupported target for method call.", node->target->location);
    }
    
    // Handle "Program.Main" -> "main" if called internally (unlikely for this example)
    if (func_name_str == "Program.Main") func_name_str = "main";

    llvm::Function* callee = m_module->getFunction(func_name_str);
    if (!callee) {
        log_error("Call to undefined function: " + func_name_str, node->target->location);
    }
    std::vector<llvm::Value*> args_values;
    if (node->arguments) {
        for (const auto& arg_node : node->arguments->arguments) {
            args_values.push_back(visit(arg_node->expression));
        }
    }
    if (callee->arg_size() != args_values.size()) {
        log_error("Incorrect argument count for call to " + func_name_str, node->location);
    }
    // Arg type checking would be here.
    return m_builder->CreateCall(callee, args_values, "calltmp");
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<ObjectCreationExpressionNode> node) {
    log_error("'new' expressions are not yet supported.", node->location);
    return nullptr;
}

llvm::Value* ScriptCompiler::visit(std::shared_ptr<ThisExpressionNode> node) {
    log_error("'this' keyword is not supported in current static context.", node->location);
    return nullptr;
}

// --- Helper Methods ---

llvm::Type* ScriptCompiler::get_llvm_type(std::shared_ptr<TypeNameNode> type_node) {
    if (!type_node) {
        log_error("TypeNameNode is null during LLVM type lookup.");
    }
    return get_llvm_type_from_string(type_node->name);
}

llvm::Type* ScriptCompiler::get_llvm_type_from_string(const std::string& type_name) {
    if (type_name == "int") return llvm::Type::getInt32Ty(*m_context);
    if (type_name == "bool") return llvm::Type::getInt1Ty(*m_context);
    if (type_name == "void") return llvm::Type::getVoidTy(*m_context);
    log_error("Unknown or unsupported type name for LLVM: " + type_name);
    return nullptr;
}

llvm::AllocaInst* ScriptCompiler::create_entry_block_alloca(llvm::Function* function,
                                                          const std::string& var_name,
                                                          llvm::Type* type) {
    llvm::IRBuilder<> TmpB(&function->getEntryBlock(), function->getEntryBlock().getFirstInsertionPt());
    return TmpB.CreateAlloca(type, nullptr, var_name.c_str());
}

} // namespace Mycelium::Scripting::Lang