#include "sharpie/compiler/script_compiler.hpp"
#include "sharpie/common/logger.hpp"
#include "sharpie/compiler/codegen/codegen.hpp"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Support/CodeGen.h"

#include "runtime_binding.h"

#include <iostream>
#include <sstream>

using namespace Mycelium::Scripting::Common;
using namespace Mycelium::Scripting::Lang::CodeGen;

namespace Mycelium::Scripting::Lang
{
    // Initialize static members
    bool ScriptCompiler::jit_initialized = false;
    bool ScriptCompiler::aot_initialized = false;

    ScriptCompiler::ScriptCompiler()
    {
        semantic_analyzer = std::make_unique<SemanticAnalyzer>();
        primitive_registry.initialize_builtin_primitives();
    }

    ScriptCompiler::~ScriptCompiler() = default;

    void ScriptCompiler::initialize_for_new_compilation(const std::string &module_name)
    {
        // Create new core LLVM objects for this compilation run.
        llvm_context = std::make_unique<llvm::LLVMContext>();
        llvm_module = std::make_unique<llvm::Module>(module_name, *llvm_context);
        builder = std::make_unique<llvm::IRBuilder<>>(*llvm_context);
        scope_manager = std::make_unique<ScopeManager>(builder.get(), llvm_module.get());

        // Ensure scope manager is properly reset for new compilation
        scope_manager->reset(builder.get(), llvm_module.get());

        // Reset state
        next_type_id = 0;
    }

    void ScriptCompiler::compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string &module_name)
    {
        if (!ast_root)
        {
            log_compiler_error("AST root is null. Cannot compile.");
        }

        // --- Phase 1: Semantic Analysis ---
        LOG_INFO("Running semantic analysis...", "COMPILER");
        semantic_ir = semantic_analyzer->analyze(ast_root);
        if (has_semantic_errors())
        {
            // Note: Semantic analyzer already logs detailed errors.
            throw std::runtime_error("Semantic errors detected. Halting compilation.");
        }
        LOG_INFO("Semantic analysis successful", "COMPILER");

        // --- Phase 2: LLVM IR Generation Setup ---
        initialize_for_new_compilation(module_name);
        declare_all_runtime_functions();
        assign_type_ids_to_classes();

        // --- Phase 3: Delegate to CodeGenerator ---
        LOG_INFO("Starting code generation...", "COMPILER");

        // Create the transient state needed for this codegen run
        std::map<std::string, VariableInfo> named_values;
        std::map<llvm::Function *, const SymbolTable::ClassSymbol *> function_return_class_info_map;
        std::vector<LoopContext> loop_context_stack;

        CodeGenContext context = {
            *llvm_context,
            *llvm_module,
            *builder,
            *scope_manager,
            semantic_ir->symbol_table,
            primitive_registry,
            nullptr, // current_function
            named_values,
            function_return_class_info_map,
            loop_context_stack};

        CodeGenerator generator(context);
        generator.generate(ast_root); // This runs the entire AST-to-IR process.

        LOG_INFO("Code generation complete.", "COMPILER");

        // --- Phase 4: Verification ---
        if (llvm::verifyModule(*llvm_module, &llvm::errs()))
        {
            dump_ir();
            log_compiler_error("LLVM Module verification failed.");
        }
        LOG_INFO("LLVM module verified successfully.", "COMPILER");
    }

    const SemanticIR *ScriptCompiler::get_semantic_ir() const
    {
        return semantic_ir.get();
    }

    bool ScriptCompiler::has_semantic_errors() const
    {
        return !semantic_ir || semantic_ir->has_errors();
    }

    std::string ScriptCompiler::get_ir_string() const
    {
        if (!llvm_module)
        {
            return "Error: No LLVM module has been compiled.";
        }
        std::string ir_str;
        llvm::raw_string_ostream ostream(ir_str);
        llvm_module->print(ostream, nullptr);
        return ir_str;
    }

    void ScriptCompiler::dump_ir() const
    {
        if (llvm_module)
        {
            llvm_module->print(llvm::errs(), nullptr);
        }
    }

    void ScriptCompiler::assign_type_ids_to_classes()
    {
        if (!semantic_ir)
        {
            log_compiler_error("Cannot assign type IDs: SemanticIR is null.");
        }

        auto& classes = semantic_ir->symbol_table.get_classes();
        for (auto& [class_name, class_symbol] : classes)
        {
            // Assign unique type ID to each class
            class_symbol.type_id = next_type_id++;
            
            LOG_DEBUG("Assigned type ID " + std::to_string(class_symbol.type_id) + 
                      " to class: " + class_name, "COMPILER");
        }
    }

    void ScriptCompiler::declare_all_runtime_functions()
    {
        if (!llvm_module)
        {
            log_compiler_error("LLVM module not available for declaring runtime functions.");
        }

        // Define runtime struct types if they don't exist
        llvm::StructType::create(*llvm_context, "struct.MyceliumString")->setBody({
            llvm::PointerType::get(*llvm_context, 0), // char* data
            llvm::Type::getInt64Ty(*llvm_context),    // size_t length
            llvm::Type::getInt64Ty(*llvm_context)     // size_t capacity
        });

        llvm::StructType::create(*llvm_context, "struct.MyceliumObjectHeader")->setBody({
            llvm::Type::getInt32Ty(*llvm_context),   // ref_count (atomic)
            llvm::Type::getInt32Ty(*llvm_context),   // type_id
            llvm::PointerType::get(*llvm_context, 0) // vtable*
        });

        llvm::Type *string_ptr_type = llvm::PointerType::get(*llvm_context, 0);
        llvm::Type *header_ptr_type = llvm::PointerType::get(*llvm_context, 0);

        for (const auto &binding : get_runtime_bindings())
        {
            llvm::FunctionType *func_type = binding.get_llvm_type(*llvm_context, string_ptr_type, header_ptr_type);
            if (!llvm_module->getFunction(binding.ir_function_name))
            {
                llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, binding.ir_function_name, llvm_module.get());
            }
        }
    }

    std::unique_ptr<llvm::Module> ScriptCompiler::take_module()
    {
        if (!llvm_module)
        {
            log_compiler_error("Attempted to take a null module.");
        }
        return std::move(llvm_module);
    }

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

    llvm::GenericValue ScriptCompiler::jit_execute_function(const std::string &function_name, const std::vector<llvm::GenericValue> &args)
    {
        initialize_jit_engine_dependencies();
        std::unique_ptr<llvm::Module> jitModule = take_module();
        if (!jitModule)
        {
            log_compiler_error("Failed to obtain LLVM module for JIT execution.");
        }

        std::string errStr;
        llvm::EngineBuilder engineBuilder(std::move(jitModule));
        engineBuilder.setErrorStr(&errStr);
        engineBuilder.setEngineKind(llvm::EngineKind::JIT);
        llvm::ExecutionEngine *ee = engineBuilder.create();
        if (!ee)
        {
            log_compiler_error("Failed to construct ExecutionEngine: " + errStr);
        }

        ee->finalizeObject();
        llvm::Function *funcToRun = ee->FindFunctionNamed(function_name);
        if (!funcToRun)
        {
            delete ee;
            log_compiler_error("JIT: Function '" + function_name + "' not found.");
        }

        llvm::GenericValue result = ee->runFunction(funcToRun, args);
        delete ee;
        return result;
    }

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

        std::unique_ptr<llvm::Module> aotModule = take_module();
        if (!aotModule)
        {
            log_compiler_error("No LLVM module for AOT compilation.");
        }

        auto TargetTriple = llvm::sys::getDefaultTargetTriple();
        aotModule->setTargetTriple(TargetTriple);

        std::string Error;
        auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
        if (!Target)
        {
            log_compiler_error("Failed to lookup target: " + Error);
        }

        auto CPU = llvm::sys::getHostCPUName().str();
        auto Features = "";

        llvm::TargetOptions opt;
        auto RM = std::optional<llvm::Reloc::Model>();
        auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
        aotModule->setDataLayout(TheTargetMachine->createDataLayout());

        std::error_code EC;
        llvm::raw_fd_ostream dest(output_filename, EC, llvm::sys::fs::OF_None);
        if (EC)
        {
            log_compiler_error("Could not open file '" + output_filename + "': " + EC.message());
        }

        llvm::legacy::PassManager pass;
        auto FileType = llvm::CodeGenFileType::ObjectFile;
        if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType))
        {
            log_compiler_error("TargetMachine can't emit a file of this type.");
        }

        pass.run(*aotModule);
        dest.flush();
    }
}