#include "script_compiler.hpp"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/CFG.h" // For llvm::pred_begin, llvm::pred_end

// For JIT and AOT
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h" // For JIT (though ExecutionEngine uses GenericValue directly)
#include "llvm/ExecutionEngine/MCJIT.h"     // For MCJIT if used explicitly (EngineBuilder handles it)
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/InstCombine/InstCombine.h" // Example pass
#include "llvm/Transforms/Scalar.h"                  // Example pass (like Reassociate)
#include "llvm/Transforms/Scalar/GVN.h"              // Example pass
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/CodeGen.h"

#include <iostream>
#include <sstream>
#include <fstream> // For saving object file in AOT (though raw_fd_ostream is used)

namespace Mycelium::Scripting::Lang
{

    // --- ScriptCompiler Implementation ---

    ScriptCompiler::ScriptCompiler()
    {
        llvmContext = std::make_unique<llvm::LLVMContext>();

        // Initialize once per context effectively
        if (!myceliumStringType)
        {
            // Define the MyceliumString struct: { i8*, i64, i64 } (char*, size_t, size_t)
            // Assuming size_t is 64-bit for this target. Adjust if necessary.
            // myceliumStringType = llvm::StructType::create(*llvmContext,
            //                                               {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext)), // data (char*)
            //                                                llvm::Type::getInt64Ty(*llvmContext),                              // length (size_t)
            //                                                llvm::Type::getInt64Ty(*llvmContext)},                             // capacity (size_t)
            //                                               "struct.MyceliumString");                                           // Name for IR
        
            myceliumStringType = llvm::Type::getInt64Ty(*llvmContext);
        }
    }

    ScriptCompiler::~ScriptCompiler() = default;

    void ScriptCompiler::compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string &module_name)
    {
        if (!llvmContext)
        { // Should always be true due to constructor, but good to be defensive
            llvmContext = std::make_unique<llvm::LLVMContext>();
        }
        llvmModule = std::make_unique<llvm::Module>(module_name, *llvmContext);
        llvmBuilder = std::make_unique<llvm::IRBuilder<>>(*llvmContext);

        // Initialize the MyceliumString LLVM type if it hasn't been already for this context
        if (!myceliumStringType)
        {
            // llvm::errs() << "[DEBUG] Initializing myceliumStringType in compile_ast...\n";
            myceliumStringType = llvm::StructType::create(*llvmContext,
                                                          {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext)), // data (char*)
                                                           llvm::Type::getInt64Ty(*llvmContext),                              // length (size_t)
                                                           llvm::Type::getInt64Ty(*llvmContext)},                             // capacity (size_t)
                                                          "struct.MyceliumString");                                           // Name for IR in the LLVM module

            if (!myceliumStringType)
            {
                // llvm::errs() << "  [CRITICAL DEBUG] Failed to create myceliumStringType!\n";
                log_error("Failed to initialize MyceliumString LLVM type struct."); // This will throw
            }
            else
            {
                // llvm::errs() << "  [DEBUG] myceliumStringType created: "; myceliumStringType->print(llvm::errs()); llvm::errs() << "\n";
            }
        }

        // Declare runtime functions (like string helpers) to the LLVM module
        // This must happen AFTER llvmModule is created and types (like myceliumStringType) are initialized,
        // and BEFORE visit(ast_root) which might try to use these function declarations.
        declare_string_runtime_functions();

        // Clear state for the new compilation unit
        namedValues.clear();
        currentFunction = nullptr;

        if (!ast_root)
        {
            log_error("AST root is null. Cannot compile.");
            // log_error throws, so no need to return here explicitly
        }

        // Start visiting the AST to generate IR
        // llvm::errs() << "[DEBUG] Starting AST visitation...\n";
        visit(ast_root);
        // llvm::errs() << "[DEBUG] AST visitation completed.\n";

        // Verify the generated module for correctness
        // llvm::errs() << "[DEBUG] Verifying LLVM module...\n";
        if (llvm::verifyModule(*llvmModule, &llvm::errs()))
        {
            log_error("LLVM Module verification failed. Dumping potentially corrupt IR.");
            // llvmModule->print(llvm::errs(), nullptr); // Consider uncommenting if verification fails often
            throw std::runtime_error("LLVM module verification failed.");
        }
        // llvm::errs() << "[DEBUG] LLVM module verification successful.\n";
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
            llvm::InitializeNativeTargetAsmParser(); // Though not strictly needed for basic JIT runFunction
            jit_initialized = true;
        }
    }

    llvm::GenericValue ScriptCompiler::jit_execute_function(
        const std::string &function_name,
        const std::vector<llvm::GenericValue> &args)
    {
        initialize_jit_engine_dependencies();

        if (!llvmModule)
        {
            log_error("No LLVM module available for JIT. Compile AST first.");
        }

        llvm::errs() << "Verifying module just before giving to EngineBuilder:\n";
        if (llvm::verifyModule(*this->llvmModule, &llvm::errs())) { // Use this->llvmModule before take_module
            log_error("LLVM Module verification failed BEFORE JIT. Dumping potentially corrupt IR.");
            // this->llvmModule->print(llvm::errs(), nullptr);
            // throw std::runtime_error("LLVM module verification failed before JIT."); // Or however you want to handle
        } else {
            llvm::errs() << "Module verification successful before JIT.\n";
        }

        std::string errStr;
        // EngineBuilder takes ownership of the module
        llvm::ExecutionEngine *ee = llvm::EngineBuilder(take_module())
                                        .setErrorStr(&errStr)
                                        .setEngineKind(llvm::EngineKind::JIT) // Or ::MCJIT for more features
                                        .create();

        if (!ee)
        {
            log_error("Failed to construct ExecutionEngine: " + errStr);
        }

        // Find the function. Note: name might be mangled or adjusted (e.g., to "main")
        llvm::Function *funcToRun = ee->FindFunctionNamed(function_name);
        if (!funcToRun)
        {
            delete ee; // Important to clean up
            log_error("JIT: Function '" + function_name + "' not found in module.");
        }

        llvm::errs() << "JIT: Found function: " << funcToRun->getName() << "\n";
        if (funcToRun->isDeclaration()) {
            llvm::errs() << "JIT WARNING: funcToRun is only a declaration!\n";
        }
        llvm::FunctionType* fType = funcToRun->getFunctionType();
        if (!fType) {
            llvm::errs() << "JIT CRITICAL: funcToRun has a NULL FunctionType!\n";
            delete ee;
            log_error("JIT: Function '" + function_name + "' has a null function type.");
        }
        llvm::Type* retType = fType->getReturnType();
        if (!retType) {
            llvm::errs() << "JIT CRITICAL: funcToRun has a NULL ReturnType!\n";
            delete ee;
            log_error("JIT: Function '" + function_name + "' has a null return type.");
        }
        llvm::errs() << "JIT: funcToRun return type: "; retType->print(llvm::errs()); llvm::errs() << "\n";

        llvm::GenericValue result;
        try
        {
            result = ee->runFunction(funcToRun, args);
        }
        catch (const std::exception &e)
        {
            delete ee;
            log_error("JIT: Exception during runFunction for '" + function_name + "': " + e.what());
        }
        catch (...)
        {
            delete ee;
            log_error("JIT: Unknown exception during runFunction for '" + function_name + "'.");
        }

        delete ee; // Clean up the execution engine
        return result;
    }

    // --- AOT Compilation to Object File ---
    bool aot_initialized = false;
    void ScriptCompiler::initialize_aot_engine_dependencies()
    {
        if (!aot_initialized)
        {
            llvm::InitializeAllTargets(); // More comprehensive for AOT to various targets
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

        // It's generally safer to work on a copy or re-generate if the module is modified by passes.
        // However, for direct emission after `compile_ast`, llvmModule should be fine.

        auto target_triple_str = llvm::sys::getDefaultTargetTriple();
        llvmModule->setTargetTriple(target_triple_str);

        std::string error;
        auto target = llvm::TargetRegistry::lookupTarget(target_triple_str, error);
        if (!target)
        {
            log_error("Error looking up target '" + target_triple_str + "': " + error);
        }

        auto cpu = "generic"; // Or llvm::sys::getHostCPUName() for current CPU
        auto features = "";   // Specific CPU features string

        llvm::TargetOptions opt;
        // llvm::Reloc::Model rm = llvm::Reloc::PIC_; // Position Independent Code, common for shared libs
        auto rm = std::optional<llvm::Reloc::Model>();
        llvm::TargetMachine *target_machine = target->createTargetMachine(target_triple_str, cpu, features, opt, rm);

        if (!target_machine)
        {
            log_error("Could not create TargetMachine for triple '" + target_triple_str + "'.");
        }

        llvmModule->setDataLayout(target_machine->createDataLayout());

        std::error_code ec;
        llvm::raw_fd_ostream dest(output_filename, ec, llvm::sys::fs::OF_None); // OF_None should handle binary output correctly

        if (ec)
        {
            log_error("Could not open file '" + output_filename + "': " + ec.message());
        }

        llvm::legacy::PassManager pass_manager;
        llvm::CodeGenFileType file_type = llvm::CodeGenFileType::ObjectFile;

        // Example: Add some optimization passes (optional, but good for generated code)
        // pass_manager.add(llvm::createInstructionCombiningPass());
        // pass_manager.add(llvm::createReassociatePass());
        // pass_manager.add(llvm::createGVNPass());
        // pass_manager.add(llvm::createCFGSimplificationPass()); // In "llvm/Transforms/Scalar.h"

        if (target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, file_type))
        {
            log_error("TargetMachine can't emit a file of type 'ObjectFile'.");
        }

        pass_manager.run(*llvmModule); // Run passes, including emission
        dest.flush();                  // Ensure all data is written
        // dest.close(); // raw_fd_ostream closes on destruction

        // llvm::outs() << "Object file '" << output_filename << "' emitted successfully.\n";
        // The module is effectively "consumed" by this process in terms of its state
        // if passes modified it. If you need to JIT *after* this, you might need to re-compile AST
        // or clone the module before AOT.
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

        // Create the function declaration. It will not have a body.
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());

        // Set names for LLVM function arguments for better IR readability.
        // No allocas or stores are needed for declarations.
        unsigned param_idx = 0;
        for (auto &llvm_arg : function->args())
        {
            // Ensure we have a corresponding AST parameter node.
            if (param_idx >= node->parameters.size())
            {
                // This would indicate an internal inconsistency, but good to be safe.
                log_error("Internal Compiler Error: Mismatch between LLVM arguments and AST parameters for external function " + function->getName().str() + ". Too few AST parameters.", node->location);
                break;
            }
            const auto &ast_parameter_node = node->parameters[param_idx];
            std::string ast_param_name = ast_parameter_node->name->name;
            llvm_arg.setName(ast_param_name); // Set name of the LLVM Argument
            param_idx++;
        }

        // Verify that all AST parameters were processed if needed, similar to defined functions.
        if (param_idx != node->parameters.size())
        {
            log_error("Internal Compiler Error: Mismatch between LLVM arguments and AST parameters for external function " + function->getName().str() + ". Too many AST parameters or loop terminated early.", node->location);
        }

        // DO NOT:
        // - Set currentFunction = function; (This function is not being *defined* here)
        // - Create any BasicBlocks for this function.
        // - Set llvmBuilder->SetInsertPoint().
        // - Create allocas for parameters or add them to namedValues.
        //   namedValues is for the symbol table of the function currently being defined.
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
                { // Allow constructors which aren't static
                    // log_error("Skipping non-static method (not supported in this example): " + class_name + "." + methodDecl->name, methodDecl->location);
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
        namedValues.clear(); // Clear symbol table for new function scope

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
        { // Special case for "main"
            func_name = "main";
        }

        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());
        currentFunction = function;
        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);

        // Set names for LLVM function arguments and create allocas for them.
        // Store the AllocaInst* in namedValues.
        unsigned param_idx = 0;
        for (auto &llvm_arg : function->args())
        {
            // Ensure we have a corresponding AST parameter node. This should hold if FunctionType was built correctly.
            if (param_idx >= node->parameters.size())
            {
                log_error("Internal Compiler Error: Mismatch between LLVM arguments and AST parameters for function " + function->getName().str() + ". Too few AST parameters.", node->location);
                break;
            }
            const auto &ast_parameter_node = node->parameters[param_idx];
            std::string ast_param_name = ast_parameter_node->name->name;

            llvm_arg.setName(ast_param_name); // Set name of the LLVM Argument

            // Create an alloca for this parameter in the entry block.
            // This allows the parameter to be mutable, like a local variable.
            llvm::AllocaInst *alloca = create_entry_block_alloca(function, ast_param_name, llvm_arg.getType());

            // Store the initial value of the argument (from llvm_arg) into the alloca.
            llvmBuilder->CreateStore(&llvm_arg, alloca);

            // Add the alloca to the symbol table for the current function scope.
            if (namedValues.count(ast_param_name))
            {
                // This should ideally be caught by the parser if duplicate param names are disallowed.
                log_error("Duplicate parameter name '" + ast_param_name + "' in function " + function->getName().str(), ast_parameter_node->location);
            }
            namedValues[ast_param_name] = alloca;

            param_idx++;
        }

        // Verify that all AST parameters were processed.
        // This check ensures that the number of arguments in FunctionType matches node->parameters.
        if (param_idx != node->parameters.size())
        {
            log_error("Internal Compiler Error: Mismatch between LLVM arguments and AST parameters for function " + function->getName().str() + ". Too many AST parameters or loop terminated early.", node->location);
        }

        if (node->body)
        {
            visit(node->body.value());
            // Ensure the function's basic blocks are properly terminated.
            if (llvmBuilder->GetInsertBlock()->getTerminator() == nullptr)
            {
                if (return_type->isVoidTy())
                {
                    llvmBuilder->CreateRetVoid();
                }
                else
                {
                    // For non-void functions, if no return is found, it's an error.
                    // LLVM's verifyFunction will catch this if the last block is not terminated.
                    // However, we can be more explicit.
                    // It's often better to let verifyFunction catch this, as some paths might return correctly.
                    // If we reach here, it means the current block (likely the end of the function) is not terminated.
                    log_error("Non-void function '" + function->getName().str() + "' does not have a return statement on all control paths.", node->body.value()->location); // Or node->location
                }
            }
        }
        else
        {
            log_error("Method '" + function->getName().str() + "' has no body.", node->location);
            // If a function has no body (e.g. an extern declaration), it shouldn't reach here
            // or it should be handled differently (no entry block, no body visit).
            // For this compiler, methods are expected to have bodies.
            // We might still want to create a RetVoid if it's a void function for safety,
            // but an error is more appropriate if a body is mandatory.
            if (return_type->isVoidTy() && !entry_block->getTerminator())
            {
                llvmBuilder->CreateRetVoid(); // Gracefully terminate if possible, though error is primary
            }
        }

        if (llvm::verifyFunction(*function, &llvm::errs()))
        {
            std::string f_name_for_err = function->getName().str();
            // Dump module on verification error for more llvmContext
            // llvm::errs() << "Dumping module due to function verification error:\n";
            // llvmModule->print(llvm::errs(), nullptr);
            log_error("LLVM Function verification failed for: " + f_name_for_err + ". Check IR output above.", node->location);
            // Note: log_error throws, so the runtime_error below is redundant if log_error is used.
            // throw std::runtime_error("LLVM function '" + f_name_for_err + "' verification failed.");
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
        } // Else, merge_bb is dead, LLVM DCE will handle.
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
                long long val = std::stoll(node->valueText);
                return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvmContext), val, true);
            }
            catch (const std::exception &e)
            {
                log_error("Invalid integer literal '" + node->valueText + "': " + e.what(), node->location);
            }
        }
        case LiteralKind::Float:
        {
            try
            {
                double val = std::stod(node->valueText);
                return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*llvmContext), val);
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
        llvm::errs() << "[DEBUG TEMPORARY] Visiting string literal, expecting i64 from new_from_literal.\n";
        llvm::Function *new_string_func = llvmModule->getFunction("Mycelium_String_new_from_literal");
        if (!new_string_func) { /* ... log_error ... */ }

        llvm::Constant *str_literal_ptr = llvmBuilder->CreateGlobalStringPtr(node->valueText, "str_lit");
        std::vector<llvm::Value *> args_values;
        args_values.push_back(str_literal_ptr);
        args_values.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), node->valueText.length()));

        // The call now returns an i64 according to our temporary IR declaration
        llvm::Value* result_handle = llvmBuilder->CreateCall(new_string_func, args_values, "new_my_str_handle");
        // Ensure the type matches what the 'store' expects if `str` alloca changes
        return result_handle;
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
            // This typically means an error occurred in visiting an operand.
            // The error should have been logged there, but we can't proceed.
            log_error("One or both operands for binary expression '" + node->operatorToken->text +
                          "' failed to generate LLVM Value. See previous errors.",
                      node->location);
            return nullptr; // Should be unreachable if log_error throws
        }

        llvm::Type *LType = L->getType();
        llvm::Type *RType = R->getType();
        llvm::Type *myceliumStringPtrTy = getMyceliumStringPtrTy(); // Get our string pointer type

        // --- String Concatenation Check (Highest Precedence for '+' with strings) ---
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
                // TODO: Handle implicit conversion from other types to string for concatenation
                // e.g., string + int or int + string.
                // This would involve:
                // 1. Identifying the non-string operand.
                // 2. Calling a runtime function (e.g., "Mycelium_Int_to_String") to convert it.
                // 3. Calling Mycelium_String_concat with the two (now string) operands.
                // For now, we'll error if one is string and the other is not, unless it's explicitly handled.
                log_error("Implicit conversion for string concatenation with non-string type ('" +
                              (LIsString ? llvm_type_to_string(RType) : llvm_type_to_string(LType)) +
                              "') is not yet supported. Both operands must be strings or have explicit conversions.",
                          node->location);
                return nullptr; // Error logged and thrown
            }
            // If neither is a string, fall through to numeric addition logic.
        }

        // --- Type Coercion and Promotion Logic for Numeric and Other Types ---
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
                L = llvmBuilder->CreateSExt(L, RType, "lhs.sext"); // Sign-extend L to R's type
                commonType = RType;
            }
            else
            {                                                      // RBits < LBits (or LBits == RBits, though covered by the first if)
                R = llvmBuilder->CreateSExt(R, LType, "rhs.sext"); // Sign-extend R to L's type
                commonType = LType;
            }
        }
        else if (LType->isFloatingPointTy() && RType->isFloatingPointTy())
        {
            // Promote float to double if they are mixed
            if (LType->isFloatTy() && RType->isDoubleTy())
            {
                L = llvmBuilder->CreateFPExt(L, RType, "lhs.fpext"); // Extend L (float) to R (double)
                commonType = RType;
            }
            else if (LType->isDoubleTy() && RType->isFloatTy())
            {
                R = llvmBuilder->CreateFPExt(R, LType, "rhs.fpext"); // Extend R (float) to L (double)
                commonType = LType;
            }
            else
            {
                // Both are float, or both are double (already handled by LType == RType)
                commonType = LType;
            }
        }
        else if (LType->isIntegerTy() && RType->isFloatingPointTy())
        {
            // Convert Integer operand L to the FloatingPoint type of R
            L = llvmBuilder->CreateSIToFP(L, RType, "lhs.sitofp"); // Signed Integer to Floating Point
            commonType = RType;
        }
        else if (LType->isFloatingPointTy() && RType->isIntegerTy())
        {
            // Convert Integer operand R to the FloatingPoint type of L
            R = llvmBuilder->CreateSIToFP(R, LType, "rhs.sitofp"); // Signed Integer to Floating Point
            commonType = LType;
        }
        else
        {
            // Incompatible types (e.g., one is void, or other non-numeric/non-coercible types)
            // This also catches cases like string + non-string if not handled by specific string logic above.
            log_error("Incompatible types for binary operator '" + node->operatorToken->text +
                          "': LHS is " + llvm_type_to_string(LType) +
                          ", RHS is " + llvm_type_to_string(RType),
                      node->location);
            return nullptr; // Should be unreachable
        }

        // Ensure commonType is valid for numeric/comparison operations
        // (This check might be too restrictive if commonType could be a string pointer after some future type system change,
        //  but for now, it's for numeric/boolean operations after string logic is handled above)
        if (!commonType || (!commonType->isIntegerTy() && !commonType->isFloatingPointTy()))
        {
            // This should ideally be caught by the specific log_error above,
            // but serves as a safeguard.
            log_error("Binary operator '" + node->operatorToken->text +
                          "' operands coerced to an unsupported common type: " + llvm_type_to_string(commonType),
                      node->location);
            return nullptr; // Should be unreachable
        }

        bool isFP = commonType->isFloatingPointTy();
        bool isInt = commonType->isIntegerTy(); // True for i1 (bool) as well.

        // --- Generate LLVM Instruction based on Operator Kind ---
        switch (node->opKind)
        {
        // Arithmetic Operators
        case BinaryOperatorKind::Add: // Handled by string logic above if types are string. Numeric only here.
            if (isInt)
                return llvmBuilder->CreateAdd(L, R, "addtmp");
            if (isFP)
                return llvmBuilder->CreateFAdd(L, R, "faddtmp");
            // If we reach here for Add, it means types were not string, and not numeric int/fp after coercion.
            // This should have been caught by the commonType check or the incompatible types error.
            log_error("Operator '+' not supported for resolved common type " + llvm_type_to_string(commonType) +
                          " (operands were not strings, and not compatible numeric types).",
                      node->location);
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
                return llvmBuilder->CreateSRem(L, R, "remtmp"); // Signed remainder
            log_error("Modulo operator ('" + node->operatorToken->text +
                          "') is only supported for integer types. Got common type: " + llvm_type_to_string(commonType),
                      node->location);
            break;

        // Comparison Operators (result is always i1 - boolean)
        case BinaryOperatorKind::Equals: // ==
            // TODO: String equality should call a runtime function like Mycelium_String_equals(s1, s2)
            // For now, this will do pointer comparison for strings if they are passed through, which is incorrect semantics.
            if (LType == myceliumStringPtrTy && RType == myceliumStringPtrTy)
            {
                // Placeholder: This is pointer comparison. For value comparison, call a runtime helper.
                llvm::Function *equals_func = llvmModule->getFunction("Mycelium_String_equals"); // Needs to be declared and implemented
                if (equals_func)
                {
                    // Assuming Mycelium_String_equals returns i1 (0 for false, 1 for true)
                    // return llvmBuilder->CreateCall(equals_func, {L, R}, "streqtmp");
                }
                else
                {
                    log_error("Runtime function Mycelium_String_equals not found. String equality '==' not fully supported yet.", node->location);
                }
                // Fallback to pointer comparison with a warning, or error out completely.
                // For now, let it fall through to standard ICmpEQ which will be pointer comparison for strings.
            }
            if (isInt) // This includes i1 for bool == bool
                return llvmBuilder->CreateICmpEQ(L, R, "eqtmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOEQ(L, R, "feqtmp");
            // If commonType is pointer (like string pointers) and not explicitly handled above, ICmpEQ will compare pointers.
            if (commonType->isPointerTy())
                return llvmBuilder->CreateICmpEQ(L, R, "ptreqtmp");
            break;
        case BinaryOperatorKind::NotEquals: // !=
            // TODO: String inequality is !(Mycelium_String_equals(s1, s2))
            if (LType == myceliumStringPtrTy && RType == myceliumStringPtrTy)
            {
                // Placeholder, similar to Equals
                llvm::Function *equals_func = llvmModule->getFunction("Mycelium_String_equals");
                if (equals_func)
                {
                    // llvm::Value* eq_result = llvmBuilder->CreateCall(equals_func, {L, R}, "streq_for_ne_tmp");
                    // return llvmBuilder->CreateICmpNE(eq_result, llvm::ConstantInt::get(llvm::Type::getInt1Ty(*llvmContext), 1), "strnetmp");
                }
                else
                {
                    log_error("Runtime function Mycelium_String_equals not found. String inequality '!=' not fully supported yet.", node->location);
                }
            }
            if (isInt)
                return llvmBuilder->CreateICmpNE(L, R, "netmp");
            if (isFP)
                return llvmBuilder->CreateFCmpONE(L, R, "fnetmp");
            if (commonType->isPointerTy())
                return llvmBuilder->CreateICmpNE(L, R, "ptrnetmp");
            break;

        // Relational operators are generally not defined for strings directly, unless lexicographical
        // comparison is implemented via runtime calls. For now, they apply to numeric types.
        case BinaryOperatorKind::LessThan: // <
            if (isInt)
                return llvmBuilder->CreateICmpSLT(L, R, "lttmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOLT(L, R, "flttmp");
            log_error("Operator '<' not supported for common type " + llvm_type_to_string(commonType), node->location);
            break;
        case BinaryOperatorKind::GreaterThan: // >
            if (isInt)
                return llvmBuilder->CreateICmpSGT(L, R, "gttmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOGT(L, R, "fgttmp");
            log_error("Operator '>' not supported for common type " + llvm_type_to_string(commonType), node->location);
            break;
        case BinaryOperatorKind::LessThanOrEqual: // <=
            if (isInt)
                return llvmBuilder->CreateICmpSLE(L, R, "letmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOLE(L, R, "fletmp");
            log_error("Operator '<=' not supported for common type " + llvm_type_to_string(commonType), node->location);
            break;
        case BinaryOperatorKind::GreaterThanOrEqual: // >=
            if (isInt)
                return llvmBuilder->CreateICmpSGE(L, R, "getmp");
            if (isFP)
                return llvmBuilder->CreateFCmpOGE(L, R, "fgetmp");
            log_error("Operator '>=' not supported for common type " + llvm_type_to_string(commonType), node->location);
            break;
        // Logical Operators (assume operands are boolean or coercible to boolean by now)
        // For this language, logical ops typically expect boolean (i1). If not, type system or earlier checks should handle.
        case BinaryOperatorKind::LogicalAnd: // &&
            // LLVM's 'and' instruction is bitwise. For logical &&, short-circuiting is usually implemented with branches.
            // This simple implementation here generates bitwise 'and' if both are i1, or if commonType became i1.
            // Proper short-circuiting needs control flow (if L then R else false).
            // For now, assuming operands are already i1 or will be handled by type checks.
            if (LType == llvm::Type::getInt1Ty(*llvmContext) && RType == llvm::Type::getInt1Ty(*llvmContext))
                return llvmBuilder->CreateAnd(L, R, "logandtmp");
            log_error("Operator '&&' requires boolean operands.", node->location);
            break;
        case BinaryOperatorKind::LogicalOr: // ||
            // Similar to LogicalAnd, this is bitwise 'or'. Short-circuiting needs branches.
            if (LType == llvm::Type::getInt1Ty(*llvmContext) && RType == llvm::Type::getInt1Ty(*llvmContext))
                return llvmBuilder->CreateOr(L, R, "logortmp");
            log_error("Operator '||' requires boolean operands.", node->location);
            break;

        default:
            log_error("Unsupported binary operator kind encountered in compiler: '" + node->operatorToken->text + "'",
                      node->location);
            break; // Error is thrown
        }

        // If we reach here, it means the operator was a recognized kind,
        // but the commonType was not handled by the specific if (isInt) or if (isFP)
        // This indicates an internal logic error in the switch cases above if commonType was valid numeric.
        // Or, it's an unsupported operation for the given (coerced) types.
        log_error("Operator '" + node->operatorToken->text +
                      "' not implemented for the resolved common type '" + llvm_type_to_string(commonType) + "' (LHS: " + llvm_type_to_string(LType) + ", RHS: " + llvm_type_to_string(RType) + ")",
                  node->location);

        llvm_unreachable("BinaryExpressionNode visitor should have returned a value or thrown an error for all valid paths.");
        return nullptr; // Should not be reached
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
        switch (node->opKind)
        {
        case UnaryOperatorKind::LogicalNot:
            if (operand->getType() != llvm::Type::getInt1Ty(*llvmContext))
            {
                log_error("LogicalNot (!) expects boolean (i1). Got: " + llvm_type_to_string(operand->getType()), node->operand->location);
            }
            return llvmBuilder->CreateICmpEQ(operand, llvm::ConstantInt::getFalse(*llvmContext), "nottmp");
        case UnaryOperatorKind::UnaryMinus:
            if (!operand->getType()->isIntegerTy() && !operand->getType()->isFloatingPointTy())
            { // Add FP if supported
                log_error("UnaryMinus (-) expects numeric. Got: " + llvm_type_to_string(operand->getType()), node->operand->location);
            }
            if (operand->getType()->isIntegerTy())
                return llvmBuilder->CreateNeg(operand, "negtmp");
            // if (operand->getType()->isFloatingPointTy()) return llvmBuilder->CreateFNeg(operand, "fnegtmp");
            break;
        default:
            log_error("Unsupported unary op: " + node->operatorToken->text, node->location);
        }
        llvm_unreachable("Unary op visitor error.");
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<MethodCallExpressionNode> node)
    {
        std::string func_name_str; // Determine from node->target
        // This part needs robust implementation based on how methods are identified (static, instance)
        if (auto idTarget = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target))
        {
            func_name_str = idTarget->identifier->name; // Global/static assumed
        }
        else if (auto memberAccess = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target))
        {
            if (auto classId = std::dynamic_pointer_cast<IdentifierExpressionNode>(memberAccess->target))
            {
                func_name_str = classId->identifier->name + "." + memberAccess->memberName->name;
            }
            else
            {
                log_error("Instance method calls not yet supported for member access target.", memberAccess->target->location);
            }
        }
        else
        {
            log_error("Unsupported target for method call.", node->target->location);
        }

        // Handle "Program.Main" -> "main" if called internally (unlikely for this example)
        if (func_name_str == "Program.Main")
            func_name_str = "main";

        llvm::Function *callee = llvmModule->getFunction(func_name_str);
        if (!callee)
        {
            log_error("Call to undefined function: " + func_name_str, node->target->location);
        }
        std::vector<llvm::Value *> args_values;
        if (node->argumentList)
        {
            for (const auto &arg_node : node->argumentList->arguments)
            {
                args_values.push_back(visit(arg_node->expression));
            }
        }
        if (callee->arg_size() != args_values.size())
        {
            log_error("Incorrect argument count for call to " + func_name_str, node->location);
        }
        // Arg type checking would be here.
        return llvmBuilder->CreateCall(callee, args_values, "calltmp");
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
            return llvm::Type::getInt64Ty(*llvmContext);

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

    void ScriptCompiler::declare_string_runtime_functions()
    {
        if (!llvmModule)
            return;

        llvm::Type* stringHandleTy = llvm::Type::getInt64Ty(*llvmContext); // Our temporary "string" type
    llvm::Type* charPtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext));
    llvm::Type* sizeTTy = llvm::Type::getInt64Ty(*llvmContext); // Assuming size_t is i64
    llvm::Type* voidTy = llvm::Type::getVoidTy(*llvmContext);

    // MyceliumString* (now i64) Mycelium_String_new_from_literal(const char* c_str, size_t len)
    llvm::FunctionType *func_type_new = llvm::FunctionType::get(stringHandleTy, {charPtrTy, sizeTTy}, false);
    if (!llvm::Function::Create(func_type_new, llvm::Function::ExternalLinkage, "Mycelium_String_new_from_literal", llvmModule.get())) {
        log_error("Failed to create (simplified) LLVM func decl for Mycelium_String_new_from_literal.");
    }

    // MyceliumString* (now i64) Mycelium_String_concat(MyceliumString* s1 (i64), MyceliumString* s2 (i64))
    llvm::FunctionType *func_type_concat = llvm::FunctionType::get(stringHandleTy, {stringHandleTy, stringHandleTy}, false);
    if (!llvm::Function::Create(func_type_concat, llvm::Function::ExternalLinkage, "Mycelium_String_concat", llvmModule.get())) {
         log_error("Failed to create (simplified) LLVM func decl for Mycelium_String_concat.");
    }

    // void Mycelium_String_print(MyceliumString* str (i64))
    llvm::FunctionType* func_type_print = llvm::FunctionType::get(voidTy, {stringHandleTy}, false);
    if (!llvm::Function::Create(func_type_print, llvm::Function::ExternalLinkage, "Mycelium_String_print", llvmModule.get())) {
        log_error("Failed to create (simplified) LLVM func decl for Mycelium_String_print.");
    }

    // void Mycelium_String_delete(MyceliumString* str (i64))
    llvm::FunctionType* func_type_delete = llvm::FunctionType::get(voidTy, {stringHandleTy}, false);
    if (!llvm::Function::Create(func_type_delete, llvm::Function::ExternalLinkage, "Mycelium_String_delete", llvmModule.get())) {
        log_error("Failed to create (simplified) LLVM func decl for Mycelium_String_delete.");
    }
}

    llvm::AllocaInst *ScriptCompiler::create_entry_block_alloca(llvm::Function *function,
                                                                const std::string &var_name,
                                                                llvm::Type *type)
    {
        llvm::IRBuilder<> TmpB(&function->getEntryBlock(), function->getEntryBlock().getFirstInsertionPt());
        return TmpB.CreateAlloca(type, nullptr, var_name.c_str());
    }

} // namespace Mycelium::Scripting::Lang