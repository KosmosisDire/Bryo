#include "script_compiler.hpp"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/CFG.h" // For llvm::pred_begin, llvm::pred_end

// For JIT and AOT
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/CodeGen.h"
#include "runtime_binding.h"

#include <iostream>
#include <sstream>
#include <fstream>

namespace Mycelium::Scripting::Lang
{

    // --- ScriptCompiler Implementation ---

    ScriptCompiler::ScriptCompiler()
    {
        llvmContext = std::make_unique<llvm::LLVMContext>();
        // Don't initialize myceliumStringType here - it will be done in compile_ast
    }

    ScriptCompiler::~ScriptCompiler() = default;

    void ScriptCompiler::compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string &module_name)
    {
        if (!llvmContext)
        {
            llvmContext = std::make_unique<llvm::LLVMContext>();
        }
        llvmModule = std::make_unique<llvm::Module>(module_name, *llvmContext);
        llvmBuilder = std::make_unique<llvm::IRBuilder<>>(*llvmContext);

        // Initialize the MyceliumString LLVM type properly as a struct
        if (!myceliumStringType)
        {
            myceliumStringType = llvm::StructType::create(*llvmContext,
                                                          {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext)), // data (char*)
                                                           llvm::Type::getInt64Ty(*llvmContext),                              // length (size_t)
                                                           llvm::Type::getInt64Ty(*llvmContext)},                             // capacity (size_t)
                                                          "struct.MyceliumString");

            if (!myceliumStringType)
            {
                log_error("Failed to initialize MyceliumString LLVM type struct.");
            }
        }

        // Declare runtime functions
        declare_all_runtime_functions();

        // Clear state for the new compilation unit
        namedValues.clear();
        currentFunction = nullptr;

        if (!ast_root)
        {
            log_error("AST root is null. Cannot compile.");
        }

        // Start visiting the AST to generate IR
        visit(ast_root);

        // Verify the generated module
        if (llvm::verifyModule(*llvmModule, &llvm::errs()))
        {
            log_error("LLVM Module verification failed. Dumping potentially corrupt IR.");
            throw std::runtime_error("LLVM module verification failed.");
        }
    }

    std::string ScriptCompiler::get_ir_string() const
    {
        if (!llvmModule)
            return "[No LLVM Module Initialized]";
        std::string irString;
        llvm::raw_string_ostream ostream(irString);
        llvmModule->print(ostream, nullptr);
        ostream.flush();
        return irString;
    }

    void ScriptCompiler::dump_ir() const
    {
        if (!llvmModule)
        {
            llvm::errs() << "[No LLVM Module Initialized to dump]\n";
            return;
        }
        llvmModule->print(llvm::errs(), nullptr);
    }

    std::unique_ptr<llvm::Module> ScriptCompiler::take_module()
    {
        if (!llvmModule)
        {
            log_error("Attempted to take a null module. Compile AST first.");
        }
        return std::move(llvmModule);
    }

    [[noreturn]] void ScriptCompiler::log_error(const std::string &message, std::optional<SourceLocation> loc)
    {
        std::stringstream ss;
        ss << "Script Compiler Error";
        if (loc)
        {
            ss << " " << loc->to_string();
        }
        ss << ": " << message;
        llvm::errs() << ss.str() << "\n";
        throw std::runtime_error(ss.str());
    }

    std::string ScriptCompiler::llvm_type_to_string(llvm::Type *type) const
    {
        if (!type)
            return "<null llvm type>";
        std::string typeStr;
        llvm::raw_string_ostream rso(typeStr);
        type->print(rso);
        return rso.str();
    }

    // --- JIT Execution ---
    bool jit_initialized = false;
    void ScriptCompiler::initialize_jit_engine_dependencies()
    {
        if (!jit_initialized)
        {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm::InitializeNativeTargetAsmParser();
            jit_initialized = true;
        }
    }

    llvm::GenericValue ScriptCompiler::jit_execute_function(
        const std::string &function_name,
        const std::vector<llvm::GenericValue> &args)
    {
        initialize_jit_engine_dependencies(); // Ensures LLVM native targets are set up

        if (!llvmModule)
        { // This module is about to be taken by EngineBuilder.
          // A check here implies it might have been taken by a previous call
          // if ScriptCompiler instance is reused without recompiling.
            log_error("No LLVM module available for JIT. Compile AST first or ensure ScriptCompiler is not reused across JIT runs without recompiling.");
        }

        std::string errStr;
        llvm::EngineBuilder engineBuilder(take_module());
        engineBuilder.setErrorStr(&errStr);
        engineBuilder.setEngineKind(llvm::EngineKind::JIT); 

        llvm::ExecutionEngine *ee = engineBuilder.create();

        if (!ee)
        {
            log_error("Failed to construct ExecutionEngine: " + errStr);
        }

        llvm::errs() << "--- Explicitly mapping C runtime functions for JIT from registry ---\n";
        const auto &bindings = get_runtime_bindings();
        if (bindings.empty())
        {
            llvm::errs() << "Warning: Runtime binding registry is empty. No explicit mappings to add.\n";
        }
        for (const auto &binding : bindings)
        {
            if (binding.c_function_pointer == nullptr)
            {
                llvm::errs() << "JIT WARNING: C function pointer for '" << binding.ir_function_name
                             << "' is null in the registry! Skipping mapping.\n";
                continue;
            }
            ee->addGlobalMapping(binding.ir_function_name,
                                 reinterpret_cast<uint64_t>(binding.c_function_pointer));
            llvm::errs() << "Mapped " << binding.ir_function_name << " to address: "
                         << binding.c_function_pointer << "\n";
        }
        llvm::errs() << "-------------------------------------------------------------------\n";

        ee->finalizeObject();

        llvm::errs() << "--- Checking JIT Function Addresses for Externs (POST-MAPPING & FINALIZE) ---\n";
        bool all_externs_resolved_by_getaddress = true;
        for (const auto &binding : bindings)
        { 
            uint64_t addr = ee->getFunctionAddress(binding.ir_function_name);
            if (addr == 0)
            {
                llvm::errs() << "JIT WARNING: Address of extern function '" << binding.ir_function_name
                             << "' is STILL 0 according to getFunctionAddress after explicit mapping!\n";
                all_externs_resolved_by_getaddress = false;
            }
            else
            {
                llvm::errs() << "JIT INFO: Address of extern function '" << binding.ir_function_name
                             << "' via getFunctionAddress is 0x" << llvm::Twine::utohexstr(addr) << "\n";
            }
        }
        if (!all_externs_resolved_by_getaddress && !bindings.empty())
        {
            llvm::errs() << "Warning: getFunctionAddress still reports 0 for some externs. "
                         << "Execution will rely on addGlobalMapping having taken effect internally for the JIT.\n";
        }
        llvm::errs() << "------------------------------------------------------------------------\n";

        llvm::Function *funcToRun = ee->FindFunctionNamed(function_name);
        if (!funcToRun)
        {
            delete ee; 
            log_error("JIT: Function '" + function_name + "' not found in the JITted module by FindFunctionNamed.");
        }

        llvm::errs() << "JIT: Found function to run: '" << funcToRun->getName().str() << "'\n";
        if (funcToRun->isDeclaration())
        { 
            llvm::errs() << "JIT WARNING: funcToRun ('" << funcToRun->getName().str() << "') is only a declaration in the JITted module! Execution will likely fail.\n";
        }
        llvm::FunctionType *fType = funcToRun->getFunctionType();
        if (!fType)
        { 
            delete ee;
            log_error("JIT: Function '" + funcToRun->getName().str() + "' has a null function type.");
        }
        llvm::Type *retType = fType->getReturnType();
        if (!retType)
        { 
            delete ee;
            log_error("JIT: Function '" + funcToRun->getName().str() + "' has a null return type.");
        }
        llvm::errs() << "JIT: funcToRun ('" << funcToRun->getName().str() << "') expected return type: ";
        retType->print(llvm::errs());
        llvm::errs() << "\n";

        llvm::GenericValue result; 
        try
        {
            result = ee->runFunction(funcToRun, args);
        }
        catch (const std::exception &e)
        {
            delete ee;
            log_error("JIT: Exception during runFunction for '" + funcToRun->getName().str() + "': " + std::string(e.what()));
        }
        catch (...)
        {
            delete ee;
            log_error("JIT: Unknown exception during runFunction for '" + funcToRun->getName().str() + "'.");
        }

        delete ee;

        return result;
    }

    // --- AOT Compilation to Object File ---
    bool aot_initialized = false;
    void ScriptCompiler::initialize_aot_engine_dependencies()
    {
        if (!aot_initialized)
        {
            llvm::InitializeAllTargets();
            llvm::InitializeAllTargetMCs();
            llvm::InitializeAllAsmPrinters();
            llvm::InitializeAllAsmParsers();
            aot_initialized = true;
        }
    }

    void ScriptCompiler::compile_to_object_file(const std::string &output_filename)
    {
        initialize_aot_engine_dependencies();

        if (!llvmModule)
        {
            log_error("No LLVM module available for AOT compilation. Compile AST first.");
        }

        auto target_triple_str = llvm::sys::getDefaultTargetTriple();
        llvmModule->setTargetTriple(target_triple_str);

        std::string error;
        auto target = llvm::TargetRegistry::lookupTarget(target_triple_str, error);
        if (!target)
        {
            log_error("Error looking up target '" + target_triple_str + "': " + error);
        }

        auto cpu = "generic";
        auto features = "";

        llvm::TargetOptions opt;
        auto rm = std::optional<llvm::Reloc::Model>();
        llvm::TargetMachine *target_machine = target->createTargetMachine(target_triple_str, cpu, features, opt, rm);

        if (!target_machine)
        {
            log_error("Could not create TargetMachine for triple '" + target_triple_str + "'.");
        }

        llvmModule->setDataLayout(target_machine->createDataLayout());

        std::error_code ec;
        llvm::raw_fd_ostream dest(output_filename, ec, llvm::sys::fs::OF_None);

        if (ec)
        {
            log_error("Could not open file '" + output_filename + "': " + ec.message());
        }

        llvm::legacy::PassManager pass_manager;
        llvm::CodeGenFileType file_type = llvm::CodeGenFileType::ObjectFile;

        if (target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, file_type))
        {
            log_error("TargetMachine can't emit a file of type 'ObjectFile'.");
        }

        pass_manager.run(*llvmModule);
        dest.flush();
    }

    // --- Visitor Dispatchers ---

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<AstNode> node)
    {
        if (!node)
        {
            log_error("Attempted to visit a null AST node.");
        }

        if (auto specificNode = std::dynamic_pointer_cast<CompilationUnitNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ClassDeclarationNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<BlockStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<LocalVariableDeclarationStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ExpressionStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<IfStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ReturnStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<LiteralExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<IdentifierExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<BinaryExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<AssignmentExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<UnaryExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<MethodCallExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ThisExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<CastExpressionNode>(node))
            return visit(specificNode);

        log_error("Encountered an AST node type for which a specific 'visit' method is not implemented.", node->location);
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<StatementNode> node)
    {
        return visit(std::static_pointer_cast<AstNode>(node));
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ExpressionNode> node)
    {
        return visit(std::static_pointer_cast<AstNode>(node));
    }

    // --- Top-Level Visitors ---

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<CompilationUnitNode> node)
    {
        for (const auto &member : node->externs)
        {
            if (auto externDecl = std::dynamic_pointer_cast<ExternalMethodDeclarationNode>(member))
            {
                visit(externDecl);
            }
            else
            {
                log_error("Unsupported top-level member. Expected ExternalMethodDeclarationNode.", member->location);
            }
        }

        for (const auto &member : node->members)
        {
            if (auto classDecl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member))
            {
                visit(classDecl);
            }
            else
            {
                log_error("Unsupported top-level member. Expected ClassDeclarationNode.", member->location);
            }
        }
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<NamespaceDeclarationNode> node)
    {
        for (const auto &member : node->members)
        {
            if (auto classDecl = std::dynamic_pointer_cast<ClassDeclarationNode>(member))
            {
                visit(classDecl);
            }
            else
            {
                log_error("Unsupported namespace member. Expected ClassDeclarationNode.", member->location);
            }
        }
        return nullptr;
    }

    void ScriptCompiler::visit(std::shared_ptr<ExternalMethodDeclarationNode> node)
    {
        if (!node->type.has_value())
        {
            log_error("External method '" + node->name->name + "' lacks a return type AST node.", node->location);
        }

        std::shared_ptr<TypeNameNode> return_type_name_node = node->type.value();
        if (!return_type_name_node)
        {
            log_error("External method '" + node->name->name + "' has a null TypeNameNode for its return type.", node->location);
        }

        llvm::Type *return_type = get_llvm_type(return_type_name_node);
        std::vector<llvm::Type *> param_types;
        for (const auto &param_node : node->parameters)
        {
            if (!param_node->type)
            {
                log_error("External method '" + node->name->name + "' has a parameter '" + param_node->name->name + "' with no type.", param_node->location);
            }
            llvm::Type *param_llvm_type = get_llvm_type(param_node->type);
            param_types.push_back(param_llvm_type);
        }

        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_types, false);
        std::string func_name = node->name->name;

        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());

        unsigned param_idx = 0;
        for (auto &llvm_arg : function->args())
        {
            if (param_idx >= node->parameters.size())
            {
                log_error("Internal Compiler Error: Mismatch between LLVM arguments and AST parameters for external function " + function->getName().str() + ". Too few AST parameters.", node->location);
                break;
            }
            const auto &ast_parameter_node = node->parameters[param_idx];
            std::string ast_param_name = ast_parameter_node->name->name;
            llvm_arg.setName(ast_param_name);
            param_idx++;
        }

        if (param_idx != node->parameters.size())
        {
            log_error("Internal Compiler Error: Mismatch between LLVM arguments and AST parameters for external function " + function->getName().str() + ". Too many AST parameters or loop terminated early.", node->location);
        }
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ClassDeclarationNode> node)
    {
        std::string class_name = node->name->name;
        for (const auto &member : node->members)
        {
            if (auto methodDecl = std::dynamic_pointer_cast<MethodDeclarationNode>(member))
            {
                bool is_static = false;
                for (auto mod : methodDecl->modifiers)
                {
                    if (mod.first == ModifierKind::Static)
                    {
                        is_static = true;
                        break;
                    }
                }
                if (!is_static && methodDecl->name->name != class_name)
                {
                    continue;
                }
                visit_method_declaration(methodDecl, class_name);
            }
        }
        return nullptr;
    }

    // --- Class Member Visitors ---

    llvm::Function *ScriptCompiler::visit_method_declaration(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name)
    {
        namedValues.clear();

        if (!node->type.has_value())
        {
            log_error("Method '" + class_name + "." + node->name->name + "' lacks a return type AST node.", node->location);
        }
        std::shared_ptr<TypeNameNode> return_type_name_node = node->type.value();
        if (!return_type_name_node)
        {
            log_error("Method '" + class_name + "." + node->name->name + "' has a null TypeNameNode for its return type.", node->location);
        }
        llvm::Type *return_type = get_llvm_type(return_type_name_node);

        std::vector<llvm::Type *> param_types;
        for (const auto &param_node : node->parameters)
        {
            if (!param_node->type)
            {
                log_error("Method '" + class_name + "." + node->name->name + "' has a parameter '" + param_node->name->name + "' with no type.", param_node->location);
            }
            llvm::Type *param_llvm_type = get_llvm_type(param_node->type);
            param_types.push_back(param_llvm_type);
        }

        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_types, false);

        std::string func_name = class_name + "." + node->name->name;
        if (func_name == "Program.Main")
        {
            func_name = "main";
        }

        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());
        currentFunction = function;
        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);

        unsigned param_idx = 0;
        for (auto &llvm_arg : function->args())
        {
            if (param_idx >= node->parameters.size())
            {
                log_error("Internal Compiler Error: Mismatch between LLVM arguments and AST parameters for function " + function->getName().str() + ". Too few AST parameters.", node->location);
                break;
            }
            const auto &ast_parameter_node = node->parameters[param_idx];
            std::string ast_param_name = ast_parameter_node->name->name;

            llvm_arg.setName(ast_param_name);

            llvm::AllocaInst *alloca = create_entry_block_alloca(function, ast_param_name, llvm_arg.getType());

            llvmBuilder->CreateStore(&llvm_arg, alloca);

            if (namedValues.count(ast_param_name))
            {
                log_error("Duplicate parameter name '" + ast_param_name + "' in function " + function->getName().str(), ast_parameter_node->location);
            }
            namedValues[ast_param_name] = alloca;

            param_idx++;
        }

        if (param_idx != node->parameters.size())
        {
            log_error("Internal Compiler Error: Mismatch between LLVM arguments and AST parameters for function " + function->getName().str() + ". Too many AST parameters or loop terminated early.", node->location);
        }

        if (node->body)
        {
            visit(node->body.value());
            if (llvmBuilder->GetInsertBlock()->getTerminator() == nullptr)
            {
                if (return_type->isVoidTy())
                {
                    llvmBuilder->CreateRetVoid();
                }
                else
                {
                    log_error("Non-void function '" + function->getName().str() + "' does not have a return statement on all control paths.", node->body.value()->location);
                }
            }
        }
        else
        {
            log_error("Method '" + function->getName().str() + "' has no body.", node->location);
            if (return_type->isVoidTy() && !entry_block->getTerminator())
            {
                llvmBuilder->CreateRetVoid();
            }
        }

        if (llvm::verifyFunction(*function, &llvm::errs()))
        {
            std::string f_name_for_err = function->getName().str();
            log_error("LLVM Function verification failed for: " + f_name_for_err + ". Check IR output above.", node->location);
        }

        currentFunction = nullptr;
        return function;
    }

    // --- Statement Visitors ---

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<BlockStatementNode> node)
    {
        llvm::Value *last_val = nullptr;
        for (const auto &stmt : node->statements)
        {
            if (llvmBuilder->GetInsertBlock()->getTerminator())
            {
                break;
            }
            last_val = visit(stmt);
        }
        return last_val;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<LocalVariableDeclarationStatementNode> node)
    {
        llvm::Type *var_type = get_llvm_type(node->type);
        for (const auto &declarator : node->declarators)
        {
            if (namedValues.count(declarator->name->name))
            {
                log_error("Variable '" + declarator->name->name + "' redeclared.", declarator->location);
            }
            llvm::AllocaInst *alloca = create_entry_block_alloca(currentFunction, declarator->name->name, var_type);
            namedValues[declarator->name->name] = alloca;
            if (declarator->initializer)
            {
                llvm::Value *init_val = visit(declarator->initializer.value());
                if (init_val->getType() != var_type)
                {
                    if (var_type->isIntegerTy() && init_val->getType()->isIntegerTy())
                    {
                        if (var_type->getIntegerBitWidth() > init_val->getType()->getIntegerBitWidth())
                            init_val = llvmBuilder->CreateSExt(init_val, var_type, "init.sext");
                        else if (var_type->getIntegerBitWidth() < init_val->getType()->getIntegerBitWidth())
                            init_val = llvmBuilder->CreateTrunc(init_val, var_type, "init.trunc");
                    }
                    else
                    {
                        log_error("Type mismatch for initializer of '" + declarator->name->name + "'. Expected " +
                                      llvm_type_to_string(var_type) + ", got " + llvm_type_to_string(init_val->getType()),
                                  declarator->initializer.value()->location);
                    }
                }
                llvmBuilder->CreateStore(init_val, alloca);
            }
        }
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ExpressionStatementNode> node)
    {
        return visit(node->expression);
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<IfStatementNode> node)
    {
        llvm::Value *condition_val = visit(node->condition);
        if (!condition_val)
        {
            log_error("Condition expression for if-statement failed to generate LLVM Value.", node->condition->location);
        }
        if (condition_val->getType()->isIntegerTy() && condition_val->getType() != llvm::Type::getInt1Ty(*llvmContext))
        {
            condition_val = llvmBuilder->CreateICmpNE(condition_val, llvm::ConstantInt::get(condition_val->getType(), 0, false), "cond.tobool");
        }
        else if (condition_val->getType() != llvm::Type::getInt1Ty(*llvmContext))
        {
            log_error("If condition must be boolean or integer. Got: " + llvm_type_to_string(condition_val->getType()), node->condition->location);
        }

        llvm::Function *func = llvmBuilder->GetInsertBlock()->getParent();
        llvm::BasicBlock *then_bb = llvm::BasicBlock::Create(*llvmContext, "then", func);
        llvm::BasicBlock *else_bb = node->elseStatement ? llvm::BasicBlock::Create(*llvmContext, "else", func) : nullptr;
        llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(*llvmContext, "ifcont", func);

        bool then_path_terminated, else_path_terminated = true;

        if (node->elseStatement)
        {
            llvmBuilder->CreateCondBr(condition_val, then_bb, else_bb);
        }
        else
        {
            llvmBuilder->CreateCondBr(condition_val, then_bb, merge_bb);
        }

        llvmBuilder->SetInsertPoint(then_bb);
        visit(node->thenStatement);
        then_path_terminated = (then_bb->getTerminator() != nullptr);
        if (!then_path_terminated)
            llvmBuilder->CreateBr(merge_bb);

        if (node->elseStatement)
        {
            llvmBuilder->SetInsertPoint(else_bb);
            visit(node->elseStatement.value());
            else_path_terminated = (else_bb->getTerminator() != nullptr);
            if (!else_path_terminated)
                llvmBuilder->CreateBr(merge_bb);
        }

        if (llvm::pred_begin(merge_bb) != llvm::pred_end(merge_bb))
        {
            llvmBuilder->SetInsertPoint(merge_bb);
        }
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ReturnStatementNode> node)
    {
        if (node->expression)
        {
            llvm::Value *ret_val = visit(node->expression.value());
            llvm::Type *expected_return_type = currentFunction->getReturnType();
            if (ret_val->getType() != expected_return_type)
            {
                if (expected_return_type->isIntegerTy() && ret_val->getType()->isIntegerTy())
                {
                    if (expected_return_type->getIntegerBitWidth() > ret_val->getType()->getIntegerBitWidth())
                        ret_val = llvmBuilder->CreateSExt(ret_val, expected_return_type, "ret.sext");
                    else if (expected_return_type->getIntegerBitWidth() < ret_val->getType()->getIntegerBitWidth())
                        ret_val = llvmBuilder->CreateTrunc(ret_val, expected_return_type, "ret.trunc");
                }
                else
                {
                    log_error("Return type mismatch. Function '" + std::string(currentFunction->getName()) +
                                  "' expects " + llvm_type_to_string(expected_return_type) + ", but got " + llvm_type_to_string(ret_val->getType()),
                              node->location);
                }
            }
            llvmBuilder->CreateRet(ret_val);
        }
        else
        {
            if (!currentFunction->getReturnType()->isVoidTy())
            {
                log_error("Non-void function '" + std::string(currentFunction->getName()) + "' must return a value.", node->location);
            }
            llvmBuilder->CreateRetVoid();
        }
        return nullptr;
    }

    // --- Expression Visitors ---

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<LiteralExpressionNode> node)
    {
        switch (node->kind)
        {
        case LiteralKind::Integer:
        {
            try
            {
                long long val_ll = std::stoll(node->valueText);
                if (val_ll < std::numeric_limits<int32_t>::min() || val_ll > std::numeric_limits<int32_t>::max()) {
                }
                return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvmContext), static_cast<int32_t>(val_ll), true);
            }
            catch (const std::out_of_range& oor)
            {
                log_error("Integer literal '" + node->valueText + "' is out of range for parsing: " + oor.what(), node->location);
            }
            catch (const std::invalid_argument& ia)
            {
                 log_error("Invalid integer literal format '" + node->valueText + "': " + ia.what(), node->location);
            }
        }
        case LiteralKind::Long: 
        {
            try
            {
                long long val = std::stoll(node->valueText);
                return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), val, true);
            }
            catch (const std::exception &e)
            {
                log_error("Invalid long literal '" + node->valueText + "': " + e.what(), node->location);
            }
        }
        case LiteralKind::Float:
        {
            try
            {
                float val = std::stof(node->valueText); 
                return llvm::ConstantFP::get(llvm::Type::getFloatTy(*llvmContext), val); 
            }
            catch (const std::exception &e)
            {
                log_error("Invalid float literal '" + node->valueText + "': " + e.what(), node->location);
            }
        }
        case LiteralKind::Double:
        {
            try
            {
                double val = std::stod(node->valueText);
                return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*llvmContext), val);
            }
            catch (const std::exception &e)
            {
                log_error("Invalid double literal '" + node->valueText + "': " + e.what(), node->location);
            }
        }
        case LiteralKind::Boolean:
        {
            if (node->valueText == "true")
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*llvmContext), 1, false);
            if (node->valueText == "false")
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*llvmContext), 0, false);
            log_error("Invalid boolean literal: " + node->valueText, node->location);
        }
        case LiteralKind::String:
        {
            llvm::Function *new_string_func = llvmModule->getFunction("Mycelium_String_new_from_literal");
            if (!new_string_func)
            {
                log_error("Runtime function Mycelium_String_new_from_literal not found in module.", node->location);
            }

            llvm::Constant *str_literal_ptr = llvmBuilder->CreateGlobalStringPtr(node->valueText, "str_lit");

            std::vector<llvm::Value *> args_values;
            args_values.push_back(str_literal_ptr);
            args_values.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), node->valueText.length()));

            llvm::Value *string_ptr = llvmBuilder->CreateCall(new_string_func, args_values, "new_my_str");
            return string_ptr;
        }
        default:
        {
            log_error("Unsupported literal kind: " + Mycelium::Scripting::Lang::to_string(node->kind), node->location);
        }
        }
        llvm_unreachable("Literal visitor error.");
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<IdentifierExpressionNode> node)
    {
        auto it = namedValues.find(node->identifier->name);
        if (it == namedValues.end())
        {
            log_error("Undefined variable: " + node->identifier->name, node->location);
        }
        return llvmBuilder->CreateLoad(it->second->getAllocatedType(), it->second, node->identifier->name.c_str());
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<BinaryExpressionNode> node)
    {
        llvm::Value *L = visit(node->left);
        llvm::Value *R = visit(node->right);

        if (!L || !R)
        {
            log_error("One or both operands for binary expression '" + node->operatorToken->text +
                          "' failed to generate LLVM Value. See previous errors.",
                      node->location);
            return nullptr;
        }

        llvm::Type *LType = L->getType();
        llvm::Type *RType = R->getType();
        llvm::PointerType *myceliumStringPtrTy = getMyceliumStringPtrTy();

        if (node->opKind == BinaryOperatorKind::Add)
        {
            bool LIsString = (LType == myceliumStringPtrTy);
            bool RIsString = (RType == myceliumStringPtrTy);

            if (LIsString && RIsString)
            {
                llvm::Function *concat_func = llvmModule->getFunction("Mycelium_String_concat");
                if (!concat_func)
                {
                    log_error("Runtime function Mycelium_String_concat not found in module.", node->location);
                }
                return llvmBuilder->CreateCall(concat_func, {L, R}, "concat_str");
            }
            else if (LIsString || RIsString)
            {
                log_error("Implicit conversion for string concatenation with non-string type ('" +
                              (LIsString ? llvm_type_to_string(RType) : llvm_type_to_string(LType)) +
                              "') is not yet supported. Both operands must be strings or have explicit conversions.",
                          node->location);
                return nullptr;
            }
        }

        if (node->opKind == BinaryOperatorKind::Equals || node->opKind == BinaryOperatorKind::NotEquals)
        {
            bool LIsString = (LType == myceliumStringPtrTy);
            bool RIsString = (RType == myceliumStringPtrTy);

            if (LIsString && RIsString)
            {
                if (node->opKind == BinaryOperatorKind::Equals)
                    return llvmBuilder->CreateICmpEQ(L, R, "streq_ptr");
                else
                    return llvmBuilder->CreateICmpNE(L, R, "strne_ptr");
            }
        }

        llvm::Type *commonType = nullptr;

        if (LType == RType)
        {
            commonType = LType;
        }
        else if (LType->isIntegerTy() && RType->isIntegerTy())
        {
            unsigned LBits = LType->getIntegerBitWidth();
            unsigned RBits = RType->getIntegerBitWidth();
            if (LBits < RBits)
            {
                L = llvmBuilder->CreateSExt(L, RType, "lhs.sext");
                commonType = RType;
            }
            else
            {
                R = llvmBuilder->CreateSExt(R, LType, "rhs.sext");
                commonType = LType;
            }
        }
        else if (LType->isFloatingPointTy() && RType->isFloatingPointTy())
        {
            if (LType->isFloatTy() && RType->isDoubleTy())
            {
                L = llvmBuilder->CreateFPExt(L, RType, "lhs.fpext");
                commonType = RType;
            }
            else if (LType->isDoubleTy() && RType->isFloatTy())
            {
                R = llvmBuilder->CreateFPExt(R, LType, "rhs.fpext");
                commonType = LType;
            }
            else
            {
                commonType = LType;
            }
        }
        else if (LType->isIntegerTy() && RType->isFloatingPointTy())
        {
            L = llvmBuilder->CreateSIToFP(L, RType, "lhs.sitofp");
            commonType = RType;
        }
        else if (LType->isFloatingPointTy() && RType->isIntegerTy())
        {
            R = llvmBuilder->CreateSIToFP(R, LType, "rhs.sitofp");
            commonType = LType;
        }
        else
        {
            log_error("Incompatible types for binary operator '" + node->operatorToken->text +
                          "': LHS is " + llvm_type_to_string(LType) +
                          ", RHS is " + llvm_type_to_string(RType),
                      node->location);
            return nullptr;
        }

        if (!commonType || (!commonType->isIntegerTy() && !commonType->isFloatingPointTy()))
        {
            log_error("Binary operator '" + node->operatorToken->text +
                          "' operands coerced to an unsupported common type: " + llvm_type_to_string(commonType),
                      node->location);
            return nullptr;
        }

        bool isFP = commonType->isFloatingPointTy();
        bool isInt = commonType->isIntegerTy();

        switch (node->opKind)
        {
        case BinaryOperatorKind::Add:
            if (isInt)
                return llvmBuilder->CreateAdd(L, R, "addtmp");
            if (isFP)
                return llvmBuilder->CreateFAdd(L, R, "faddtmp");
            break;
        case BinaryOperatorKind::Subtract:
            if (isInt)
                return llvmBuilder->CreateSub(L, R, "subtmp");
            if (isFP)
                return llvmBuilder->CreateFSub(L, R, "fsubtmp");
            break;
        case BinaryOperatorKind::Multiply:
            if (isInt)
                return llvmBuilder->CreateMul(L, R, "multmp");
            if (isFP)
                return llvmBuilder->CreateFMul(L, R, "fmultmp");
            break;
        case BinaryOperatorKind::Divide:
            if (isInt)
            {
                return llvmBuilder->CreateSDiv(L, R, "divtmp");
            }
            if (isFP)
                return llvmBuilder->CreateFDiv(L, R, "fdivtmp");
            break;
        case BinaryOperatorKind::Modulo:
            if (isInt)
                return llvmBuilder->CreateSRem(L, R, "remtmp");
            log_error("Modulo operator ('" + node->operatorToken->text +
                          "') is only supported for integer types. Got common type: " + llvm_type_to_string(commonType),
                      node->location);
            break;
        case BinaryOperatorKind::Equals:
            if (isInt)
                return llvmBuilder->CreateICmpEQ(L, R, "eqtmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOEQ(L, R, "feqtmp");
            if (commonType->isPointerTy())
                return llvmBuilder->CreateICmpEQ(L, R, "ptreqtmp");
            break;
        case BinaryOperatorKind::NotEquals:
            if (isInt)
                return llvmBuilder->CreateICmpNE(L, R, "netmp");
            if (isFP)
                return llvmBuilder->CreateFCmpONE(L, R, "fnetmp");
            if (commonType->isPointerTy())
                return llvmBuilder->CreateICmpNE(L, R, "ptrnetmp");
            break;
        case BinaryOperatorKind::LessThan:
            if (isInt)
                return llvmBuilder->CreateICmpSLT(L, R, "lttmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOLT(L, R, "flttmp");
            log_error("Operator '<' not supported for common type " + llvm_type_to_string(commonType), node->location);
            break;
        case BinaryOperatorKind::GreaterThan:
            if (isInt)
                return llvmBuilder->CreateICmpSGT(L, R, "gttmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOGT(L, R, "fgttmp");
            log_error("Operator '>' not supported for common type " + llvm_type_to_string(commonType), node->location);
            break;
        case BinaryOperatorKind::LessThanOrEqual:
            if (isInt)
                return llvmBuilder->CreateICmpSLE(L, R, "letmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOLE(L, R, "fletmp");
            log_error("Operator '<=' not supported for common type " + llvm_type_to_string(commonType), node->location);
            break;
        case BinaryOperatorKind::GreaterThanOrEqual:
            if (isInt)
                return llvmBuilder->CreateICmpSGE(L, R, "getmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOGE(L, R, "fgetmp");
            log_error("Operator '>=' not supported for common type " + llvm_type_to_string(commonType), node->location);
            break;
        case BinaryOperatorKind::LogicalAnd:
            if (LType == llvm::Type::getInt1Ty(*llvmContext) && RType == llvm::Type::getInt1Ty(*llvmContext))
                return llvmBuilder->CreateAnd(L, R, "logandtmp");
            log_error("Operator '&&' requires boolean operands.", node->location);
            break;
        case BinaryOperatorKind::LogicalOr:
            if (LType == llvm::Type::getInt1Ty(*llvmContext) && RType == llvm::Type::getInt1Ty(*llvmContext))
                return llvmBuilder->CreateOr(L, R, "logortmp");
            log_error("Operator '||' requires boolean operands.", node->location);
            break;

        default:
            log_error("Unsupported binary operator kind encountered in compiler: '" + node->operatorToken->text + "'",
                      node->location);
            break;
        }

        log_error("Operator '" + node->operatorToken->text +
                      "' not implemented for the resolved common type '" + llvm_type_to_string(commonType) + "' (LHS: " + llvm_type_to_string(LType) + ", RHS: " + llvm_type_to_string(RType) + ")",
                  node->location);

        llvm_unreachable("BinaryExpressionNode visitor should have returned a value or thrown an error for all valid paths.");
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<AssignmentExpressionNode> node)
    {
        auto id_target = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target);
        if (!id_target)
        {
            log_error("Assignment target must be a variable.", node->target->location);
        }
        auto it = namedValues.find(id_target->identifier->name);
        if (it == namedValues.end())
        {
            log_error("Assigning to undeclared variable: " + id_target->identifier->name, id_target->location);
        }
        llvm::AllocaInst *ptr_to_store = it->second;
        llvm::Value *val_to_store = visit(node->source);
        if (val_to_store->getType() != ptr_to_store->getAllocatedType())
        {
            if (ptr_to_store->getAllocatedType()->isIntegerTy() && val_to_store->getType()->isIntegerTy())
            {
                llvm::Type *expected_type = ptr_to_store->getAllocatedType();
                if (expected_type->getIntegerBitWidth() > val_to_store->getType()->getIntegerBitWidth())
                    val_to_store = llvmBuilder->CreateSExt(val_to_store, expected_type, "assign.sext");
                else if (expected_type->getIntegerBitWidth() < val_to_store->getType()->getIntegerBitWidth())
                    val_to_store = llvmBuilder->CreateTrunc(val_to_store, expected_type, "assign.trunc");
            }
            else
            {
                log_error("Type mismatch in assignment to '" + id_target->identifier->name + "'. Expected " +
                              llvm_type_to_string(ptr_to_store->getAllocatedType()) + ", got " + llvm_type_to_string(val_to_store->getType()),
                          node->location);
            }
        }
        llvmBuilder->CreateStore(val_to_store, ptr_to_store);
        return val_to_store;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<UnaryExpressionNode> node)
    {
        llvm::Value *operand = visit(node->operand);
        if (!operand) { // Check if operand compilation failed
             log_error("Operand for unary expression '" + node->operatorToken->text + "' failed to generate LLVM Value.", node->operand->location);
            return nullptr;
        }

        switch (node->opKind)
        {
        case UnaryOperatorKind::LogicalNot:
            if (operand->getType() != llvm::Type::getInt1Ty(*llvmContext))
            {
                log_error("LogicalNot (!) expects boolean (i1). Got: " + llvm_type_to_string(operand->getType()), node->operand->location);
                return nullptr; 
            }
            return llvmBuilder->CreateICmpEQ(operand, llvm::ConstantInt::getFalse(*llvmContext), "nottmp");
        case UnaryOperatorKind::UnaryMinus:
            if (!operand->getType()->isIntegerTy() && !operand->getType()->isFloatingPointTy())
            {
                log_error("UnaryMinus (-) expects numeric. Got: " + llvm_type_to_string(operand->getType()), node->operand->location);
                return nullptr;
            }
            if (operand->getType()->isIntegerTy()) {
                return llvmBuilder->CreateNeg(operand, "negtmp");
            } else if (operand->getType()->isFloatingPointTy()) {
                return llvmBuilder->CreateFNeg(operand, "fnegtmp");
            }
            // Fallthrough if somehow numeric but not int or float (should be caught by above check)
            log_error("UnaryMinus (-) applied to unsupported numeric type: " + llvm_type_to_string(operand->getType()), node->location);
            return nullptr;
        // TODO: Add UnaryPlus, PreIncrement, PostIncrement, PreDecrement, PostDecrement
        default:
            log_error("Unsupported unary op: " + node->operatorToken->text, node->location);
            return nullptr; 
        }
        // This part should ideally not be reached if all cases return or throw.
        // If a case breaks without returning (which it shouldn't for a Value-returning function),
        // or if a new UnaryOperatorKind is added without a case, this could be hit.
        log_error("Internal compiler error: Unhandled UnaryOperatorKind in visitor: " + Mycelium::Scripting::Lang::to_string(node->opKind), node->location);
        llvm_unreachable("Unary op visitor error due to unhandled case or fallthrough.");
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<MethodCallExpressionNode> node)
    {
        std::string func_name_str;

        if (auto idTarget = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target))
        {
            func_name_str = idTarget->identifier->name;
        }
        else if (auto memberAccess = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target))
        {
            if (auto classId = std::dynamic_pointer_cast<IdentifierExpressionNode>(memberAccess->target))
            {
                func_name_str = classId->identifier->name + "." + memberAccess->memberName->name;
            }
            else
            {
                log_error("Instance method calls or complex member access targets for method calls not yet fully supported.", memberAccess->target->location);
                return nullptr;
            }
        }
        else
        {
            log_error("Unsupported target type for method call.", node->target->location);
            return nullptr;
        }

        if (func_name_str == "Program.Main")
        { 
            func_name_str = "main";
        }

        llvm::Function *callee = llvmModule->getFunction(func_name_str);
        if (!callee)
        {
            log_error("Call to undefined function: " + func_name_str, node->target->location);
            return nullptr;
        }

        std::vector<llvm::Value *> args_values;
        if (node->argumentList)
        {
            unsigned arg_idx = 0;
            for (const auto &arg_node : node->argumentList->arguments)
            {
                llvm::Value *arg_llvm_val = visit(arg_node->expression);
                if (!arg_llvm_val)
                {
                    log_error("Argument expression " + std::to_string(arg_idx) + " for call to " + func_name_str + " resulted in null LLVM value.", arg_node->location);
                    return nullptr;
                }

                if (arg_idx < callee->arg_size())
                {
                    llvm::Type *expected_llvm_arg_type = callee->getFunctionType()->getParamType(arg_idx);
                    if (arg_llvm_val->getType() != expected_llvm_arg_type)
                    {
                        if (expected_llvm_arg_type->isIntegerTy() && arg_llvm_val->getType()->isIntegerTy())
                        {
                            if (expected_llvm_arg_type->getIntegerBitWidth() > arg_llvm_val->getType()->getIntegerBitWidth())
                                arg_llvm_val = llvmBuilder->CreateSExt(arg_llvm_val, expected_llvm_arg_type, "arg.sext");
                            else if (expected_llvm_arg_type->getIntegerBitWidth() < arg_llvm_val->getType()->getIntegerBitWidth())
                                arg_llvm_val = llvmBuilder->CreateTrunc(arg_llvm_val, expected_llvm_arg_type, "arg.trunc");
                            else if (arg_llvm_val->getType() != expected_llvm_arg_type)
                            {
                                log_error("Type mismatch for argument " + std::to_string(arg_idx) + " in call to " + func_name_str +
                                              ". Expected " + llvm_type_to_string(expected_llvm_arg_type) +
                                              ", got " + llvm_type_to_string(arg_llvm_val->getType()),
                                          arg_node->location);
                                return nullptr;
                            }
                        }
                        else if (expected_llvm_arg_type->isPointerTy() && arg_llvm_val->getType()->isPointerTy())
                        {
                            if (expected_llvm_arg_type != arg_llvm_val->getType())
                            {
                                log_error("Pointer type mismatch for argument " + std::to_string(arg_idx) + " in call to " + func_name_str +
                                              ". Expected " + llvm_type_to_string(expected_llvm_arg_type) +
                                              ", got " + llvm_type_to_string(arg_llvm_val->getType()),
                                          arg_node->location);
                                return nullptr;
                            }
                        }
                        else
                        { 
                            log_error("Type mismatch for argument " + std::to_string(arg_idx) + " in call to " + func_name_str +
                                          ". Expected " + llvm_type_to_string(expected_llvm_arg_type) +
                                          ", got " + llvm_type_to_string(arg_llvm_val->getType()),
                                      arg_node->location);
                            return nullptr;
                        }
                    }
                }
                args_values.push_back(arg_llvm_val);
                arg_idx++;
            }
        }

        if (callee->arg_size() != args_values.size())
        {
            log_error("Incorrect argument count for call to " + func_name_str + ". Expected " +
                          std::to_string(callee->arg_size()) + ", got " + std::to_string(args_values.size()),
                      node->location);
            return nullptr;
        }

        if (callee->getReturnType()->isVoidTy())
        {
            return llvmBuilder->CreateCall(callee, args_values, ""); 
        }
        else
        {
            return llvmBuilder->CreateCall(callee, args_values, "calltmp");
        }
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ObjectCreationExpressionNode> node)
    {
        log_error("'new' expressions are not yet supported.", node->location);
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ThisExpressionNode> node)
    {
        log_error("'this' keyword is not supported in current static llvmContext.", node->location);
        return nullptr;
    }

    llvm::Value* ScriptCompiler::visit(std::shared_ptr<CastExpressionNode> node) {
        if (!node || !node->targetType || !node->expression) {
            log_error("Invalid CastExpressionNode: missing target type or expression.", node ? node->location : std::nullopt);
            return nullptr;
        }

        llvm::Value* expr_value = visit(node->expression);
        if (!expr_value) {
            log_error("Expression being cast resulted in null LLVM value.", node->expression->location);
            return nullptr;
        }

        llvm::Type* source_llvm_type = expr_value->getType();
        llvm::Type* target_llvm_type = get_llvm_type(node->targetType);
        if (!target_llvm_type) {
            return nullptr;
        }

        if (source_llvm_type == target_llvm_type) {
            return expr_value; 
        }

        bool source_is_int = source_llvm_type->isIntegerTy();
        bool target_is_int = target_llvm_type->isIntegerTy();
        bool source_is_fp = source_llvm_type->isFloatingPointTy();
        bool target_is_fp = target_llvm_type->isFloatingPointTy();

        if (source_is_int && target_is_int) {
            unsigned source_bits = source_llvm_type->getIntegerBitWidth();
            unsigned target_bits = target_llvm_type->getIntegerBitWidth();
            if (target_bits < source_bits) {
                return llvmBuilder->CreateTrunc(expr_value, target_llvm_type, "cast.trunc");
            } else if (target_bits > source_bits) {
                return llvmBuilder->CreateSExt(expr_value, target_llvm_type, "cast.sext");
            }
        }
        else if (source_is_fp && target_is_fp) {
            if (target_llvm_type->isFloatTy() && source_llvm_type->isDoubleTy()) {
                return llvmBuilder->CreateFPTrunc(expr_value, target_llvm_type, "cast.fptrunc");
            } else if (target_llvm_type->isDoubleTy() && source_llvm_type->isFloatTy()) {
                return llvmBuilder->CreateFPExt(expr_value, target_llvm_type, "cast.fpext");
            }
        }
        else if (source_is_int && target_is_fp) {
            return llvmBuilder->CreateSIToFP(expr_value, target_llvm_type, "cast.sitofp");
        }
        else if (source_is_fp && target_is_int) {
            return llvmBuilder->CreateFPToSI(expr_value, target_llvm_type, "cast.fptosi");
        }

        if (source_llvm_type->isPointerTy() && target_llvm_type->isPointerTy()) {
            return llvmBuilder->CreateBitCast(expr_value, target_llvm_type, "cast.ptr.bitcast");
        }

        if (source_llvm_type->isIntegerTy(1) && target_is_int && target_llvm_type->getIntegerBitWidth() > 1) {
            return llvmBuilder->CreateZExt(expr_value, target_llvm_type, "cast.booltoint"); 
        }
        if (source_is_int && source_llvm_type->getIntegerBitWidth() > 1 && target_llvm_type->isIntegerTy(1)) {
            llvm::Value* zero = llvm::ConstantInt::get(source_llvm_type, 0, true); 
            return llvmBuilder->CreateICmpNE(expr_value, zero, "cast.inttobool");
        }

        log_error("Unsupported explicit cast from '" + llvm_type_to_string(source_llvm_type) +
                  "' to '" + llvm_type_to_string(target_llvm_type) + "'.",
                  node->location);
        return nullptr;
    }

    // --- Helper Methods ---

    llvm::Type *ScriptCompiler::get_llvm_type(std::shared_ptr<TypeNameNode> type_node)
    {
        if (!type_node)
        {
            log_error("TypeNameNode is null during LLVM type lookup.");
        }

        try
        {
            return get_llvm_type_from_string(std::get<std::shared_ptr<IdentifierNode>>(type_node->name_segment)->name);
        }
        catch (const std::bad_variant_access &e)
        {
            log_error("TypeNameNode name segment is not an IdentifierNode.", type_node->location);
        }
        catch (const std::exception &e)
        {
            log_error("Error accessing TypeNameNode name segment: " + std::string(e.what()), type_node->location);
        }
    }

    llvm::Type *ScriptCompiler::get_llvm_type_from_string(const std::string &type_name)
    {
        if (type_name == "int" || type_name == "i32")
            return llvm::Type::getInt32Ty(*llvmContext);
        if (type_name == "bool")
            return llvm::Type::getInt1Ty(*llvmContext);
        if (type_name == "void")
            return llvm::Type::getVoidTy(*llvmContext);
        if (type_name == "float" || type_name == "f32")
            return llvm::Type::getFloatTy(*llvmContext);
        if (type_name == "double" || type_name == "f64")
            return llvm::Type::getDoubleTy(*llvmContext);
        if (type_name == "char" || type_name == "i8" || type_name == "byte")
            return llvm::Type::getInt8Ty(*llvmContext);
        if (type_name == "long" || type_name == "i64")
            return llvm::Type::getInt64Ty(*llvmContext);
        if (type_name == "short" || type_name == "i16")
            return llvm::Type::getInt16Ty(*llvmContext);
        if (type_name == "string")
            return getMyceliumStringPtrTy(); 

        log_error("Unknown or unsupported type name for LLVM: " + type_name);
        return nullptr;
    }

    llvm::PointerType *ScriptCompiler::getMyceliumStringPtrTy()
    {
        if (!myceliumStringType)
        {
            log_error("MyceliumString type not initialized in LLVM context.");
        }
        return llvm::PointerType::getUnqual(myceliumStringType);
    }

    void ScriptCompiler::declare_all_runtime_functions()
    { 
        if (!llvmModule)
        {
            log_error("LLVM module not available for declaring runtime functions.");
            return;
        }
        if (!myceliumStringType && !get_runtime_bindings().empty())
        { 
            if (!myceliumStringType)
            {
                log_error("MyceliumString LLVM type not initialized before declaring runtime functions that might need it.");
                return;
            }
        }

        llvm::errs() << "--- Declaring runtime functions in LLVM IR from registry ---\n";
        for (const auto &binding : get_runtime_bindings())
        {
            llvm::FunctionType *func_type = binding.get_llvm_type(*llvmContext, getMyceliumStringPtrTy()); 
            if (!func_type)
            {
                log_error("Failed to get LLVM FunctionType for: " + binding.ir_function_name);
                continue;
            }
            llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, binding.ir_function_name, llvmModule.get());
            llvm::errs() << "Declared: " << binding.ir_function_name << "\n";
        }
        llvm::errs() << "-----------------------------------------------------------\n";
    }

    llvm::AllocaInst *ScriptCompiler::create_entry_block_alloca(llvm::Function *function,
                                                                const std::string &var_name,
                                                                llvm::Type *type)
    {
        llvm::IRBuilder<> TmpB(&function->getEntryBlock(), function->getEntryBlock().getFirstInsertionPt());
        return TmpB.CreateAlloca(type, nullptr, var_name.c_str());
    }

} // namespace Mycelium::Scripting::Lang
