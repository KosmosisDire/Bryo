#include "sharpie/compiler/script_compiler.hpp"
#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/common/logger.hpp"
#include "sharpie/script_ast.hpp" // For AST node types like CompilationUnitNode, SourceLocation

#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Casting.h"

// For JIT and AOT
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/MCJIT.h" // For EngineBuilder
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/TargetParser/Host.h" // For getDefaultTargetTriple, getHostCPUName
#include "llvm/Support/raw_ostream.h"
// #include "llvm/Transforms/InstCombine/InstCombine.h" // Not directly used by methods moved here yet
// #include "llvm/Transforms/Scalar.h"                   // Not directly used by methods moved here yet
// #include "llvm/Transforms/Scalar/GVN.h"               // Not directly used by methods moved here yet
#include "llvm/IR/IRBuilder.h"    // Used by create_entry_block_alloca
#include "llvm/Support/CodeGen.h" // For CodeGenFileType
#include "llvm/Support/DynamicLibrary.h"

// Assuming runtime_binding.h is in a path accessible to the compiler, e.g., lib/
// If it's relative to this file, the path needs adjustment.
#include "../../lib/runtime_binding.h" // Corrected path

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm> // For std::find_if

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{
    // Initialize static members
    bool ScriptCompiler::jit_initialized = false;
    bool ScriptCompiler::aot_initialized = false;

    ScriptCompiler::ScriptCompiler() : next_type_id(0)
    {
        llvmContext = std::make_unique<llvm::LLVMContext>();
        primitive_registry.initialize_builtin_primitives();
        semantic_analyzer = std::make_unique<SemanticAnalyzer>();
        scope_manager = std::make_unique<ScopeManager>(nullptr, nullptr);
    }

    ScriptCompiler::~ScriptCompiler() = default;
    void ScriptCompiler::compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string &module_name)
    {
        // --- Phase 1: Semantic Analysis ---
        run_semantic_analysis(ast_root);

        if (has_semantic_errors())
        {
            log_error("Semantic errors detected. Halting compilation.");
            return; // Stop before generating any LLVM IR
        }

        // --- Phase 2: LLVM IR Generation Setup ---
        llvmModule = std::make_unique<llvm::Module>(module_name, *llvmContext);
        llvmBuilder = std::make_unique<llvm::IRBuilder<>>(*llvmContext);

        // Update ScopeManager with the new builder and module
        scope_manager->reset(llvmBuilder.get(), llvmModule.get());

        // Set the symbol table pointer for the visitors to use
        symbolTable = &lastSemanticIR->symbol_table;

        // Initialize LLVM types
        myceliumStringType = llvm::StructType::create(*llvmContext, "struct.MyceliumString");
        myceliumObjectHeaderType = llvm::StructType::create(*llvmContext, "struct.MyceliumObjectHeader");

        // FIX: Define the bodies of the runtime structs to make them sized types
        // This resolves the "Cannot getTypeInfo() on a type that is unsized!" crash.
        // Body for MyceliumObjectHeader: { i32, i32, ptr }
        myceliumObjectHeaderType->setBody(
            {
                llvm::Type::getInt32Ty(*llvmContext),      // ref_count
                llvm::Type::getInt32Ty(*llvmContext),      // type_id
                llvm::PointerType::getUnqual(*llvmContext) // vtable*
            },
            false // isPacked
        );

        // Body for MyceliumString: { ptr, i64, i64 } (assuming 64-bit size_t)
        myceliumStringType->setBody(
            {
                llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext)), // char* data
                llvm::Type::getInt64Ty(*llvmContext),                              // size_t length
                llvm::Type::getInt64Ty(*llvmContext)                               // size_t capacity
            },
            false // isPacked
        );

        declare_all_runtime_functions();

        namedValues.clear();
        currentFunction = nullptr;
        classTypeRegistry.clear();
        functionReturnClassInfoMap.clear();
        next_type_id = 0;

        if (!ast_root)
        {
            log_error("AST root is null. Cannot compile.");
            return;
        }

        // --- Phase 3: Visit AST and Generate IR ---
        visit(ast_root);

        if (llvm::verifyModule(*llvmModule, &llvm::errs()))
        {
            log_error("LLVM Module verification failed.");
        }
    }

    std::string ScriptCompiler::get_ir_string() const
    {
        if (!llvmModule)
        {
            return "Error: No LLVM module has been compiled.";
        }
        std::string ir_str;
        llvm::raw_string_ostream ostream(ir_str);
        llvmModule->print(ostream, nullptr);
        return ir_str;
    }

    void ScriptCompiler::dump_ir() const
    {
        if (llvmModule)
        {
            llvmModule->print(llvm::errs(), nullptr);
        }
    }

    void ScriptCompiler::run_semantic_analysis(std::shared_ptr<CompilationUnitNode> ast_root)
    {
        LOG_INFO("Running semantic analysis...", "COMPILER");
        lastSemanticIR = semantic_analyzer->analyze(ast_root); // Analyze and store the result

        if (lastSemanticIR && lastSemanticIR->has_errors())
        {
            LOG_INFO("Semantic analysis complete. Errors: " + std::to_string(lastSemanticIR->errors.size()) +
                         ", Warnings: " + std::to_string(lastSemanticIR->warnings.size()),
                     "COMPILER");
        }
        else
        {
            LOG_INFO("Semantic analysis successful", "COMPILER");
        }
    }

    const SemanticIR *ScriptCompiler::getSemanticIR() const
    {
        return lastSemanticIR.get();
    }

    bool ScriptCompiler::has_semantic_errors() const
    {
        return lastSemanticIR->has_errors();
    }

    std::unique_ptr<llvm::Module> ScriptCompiler::take_module()
    {
        if (!llvmModule)
        {
            // It's better to return an empty unique_ptr than to throw from here
            // if the caller is expected to handle a null module.
            // However, the original code logs and throws. Let's keep that for now.
            log_error("Attempted to take a null module. Compile AST first.");
        }
        return std::move(llvmModule);
    }

    [[noreturn]] void ScriptCompiler::log_error(const std::string &message, std::optional<SourceLocation> loc)
    {
        std::stringstream ss;
        ss << "Script Compiler Error";
        if (loc && loc->fileName != "unknown")
        { // Check if location is meaningful
            ss << " " << loc->to_string();
        }
        ss << ": " << message;
        if (llvmModule)
        { // Dump IR if module exists, might help debug
            llvm::errs() << "Current LLVM IR state:\n";
            llvmModule->print(llvm::errs(), nullptr);
        }
        throw std::runtime_error(ss.str());
    }

    std::string ScriptCompiler::llvm_type_to_string(llvm::Type *type) const
    {
        if (!type)
            return "<null llvm type>";
        std::string typeStr;
        llvm::raw_string_ostream rso(typeStr);
        type->print(rso);
        rso.flush(); // Ensure the string is fully written
        return typeStr;
    }

    void ScriptCompiler::initialize_jit_engine_dependencies()
    {
        if (!jit_initialized)
        {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm::InitializeNativeTargetAsmParser(); // Required for MCJIT
            jit_initialized = true;
        }
    }

    llvm::GenericValue ScriptCompiler::jit_execute_function(
        const std::string &function_name,
        const std::vector<llvm::GenericValue> &args)
    {
        initialize_jit_engine_dependencies();

        if (!llvmModule)
        { // Check if llvmModule was moved or not created
            log_error("No LLVM module available for JIT. Was it compiled or taken?");
        }

        if (lastSemanticIR)
        {
            LOG_INFO("Performing runtime validation of external symbols...", "JIT");
            std::vector<std::string> missing_symbols;
            const auto& all_methods = lastSemanticIR->symbol_table.get_methods();
            for (const auto& [name, symbol] : all_methods) {
                if (symbol.is_external) {
                    // Search the current process for the symbol.
                    void* addr = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(symbol.name);
                    if (addr == nullptr) {
                        missing_symbols.push_back(symbol.name);
                    }
                }
            }

            if (!missing_symbols.empty()) {
                std::string error_msg = "The following required external functions could not be found at runtime:\n";
                for (const auto& sym : missing_symbols) {
                    error_msg += " - " + sym + "\n";
                }
                error_msg += "Please ensure they are linked or provided by the host application with 'extern \"C\"' linkage.";
                log_error(error_msg);
            }
            LOG_INFO("All external symbols validated successfully.", "JIT");
        }   

        // Create a new module for the JIT if the main one is gone, or clone.
        // For simplicity, let's assume we take_module for JIT.
        std::unique_ptr<llvm::Module> jitModule = take_module();
        if (!jitModule)
        {
            log_error("Failed to obtain LLVM module for JIT execution.");
        }

        std::string errStr;
        llvm::EngineBuilder engineBuilder(std::move(jitModule)); // Pass ownership
        engineBuilder.setErrorStr(&errStr);
        engineBuilder.setEngineKind(llvm::EngineKind::JIT);
        engineBuilder.setVerifyModules(true);
        engineBuilder.setOptLevel(llvm::CodeGenOptLevel::None);

        llvm::ExecutionEngine *ee = engineBuilder.create();

        if (!ee)
        {
            log_error("Failed to construct ExecutionEngine: " + errStr);
        }

        // Add global mappings for runtime functions
        // for (const auto &binding : get_runtime_bindings())
        // {
        //     if (binding.c_function_pointer == nullptr)
        //     {
        //         // llvm::errs() << "Skipping null C function pointer for: " << binding.ir_function_name << "\n";
        //         continue;
        //     }
        //     // llvm::errs() << "Mapping IR func: " << binding.ir_function_name << " to C func at " << binding.c_function_pointer << "\n";
        //     ee->addGlobalMapping(binding.ir_function_name, reinterpret_cast<uint64_t>(binding.c_function_pointer));
        // }

        ee->finalizeObject(); // Finalize object code before running.

        llvm::Function *funcToRun = ee->FindFunctionNamed(function_name);
        if (!funcToRun)
        {
            delete ee; // Clean up ExecutionEngine
            log_error("JIT: Function '" + function_name + "' not found in the module.");
        }

        llvm::GenericValue result;
        try
        {
            result = ee->runFunction(funcToRun, args);
        }
        catch (const std::exception &e)
        {
            delete ee;
            log_error("JIT: Exception during runFunction: " + std::string(e.what()));
        }
        catch (...)
        {
            delete ee;
            log_error("JIT: Unknown exception during runFunction.");
        }

        delete ee; // Clean up ExecutionEngine
        return result;
    }

    void ScriptCompiler::initialize_aot_engine_dependencies()
    {
        if (!aot_initialized)
        {
            // Initialize all targets and components for AOT compilation
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
            log_error("No LLVM module available for AOT compilation to object file.");
            return;
        }

        auto TargetTriple = llvm::sys::getDefaultTargetTriple();
        llvmModule->setTargetTriple(TargetTriple);

        std::string Error;
        auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

        if (!Target)
        {
            log_error("Failed to lookup target: " + Error);
            return;
        }

        auto CPU = llvm::sys::getHostCPUName().str();

        llvm::StringMap<bool> HostFeatures;
        std::string FeaturesStr;
        if (llvm::sys::getHostCPUFeatures(HostFeatures))
        {
            llvm::raw_string_ostream OS(FeaturesStr);
            bool first = true;
            for (auto &Feature : HostFeatures)
            {
                if (!first)
                {
                    OS << ",";
                }
                first = false;
                OS << (Feature.getValue() ? "+" : "-") << Feature.getKey();
            }
            OS.flush();
        }

        llvm::TargetOptions opt;
        llvm::Reloc::Model RM = llvm::Reloc::Static;
        auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, FeaturesStr, opt, RM);

        if (!TheTargetMachine)
        {
            log_error("Could not create target machine for " + TargetTriple);
            return;
        }

        llvmModule->setDataLayout(TheTargetMachine->createDataLayout());

        std::error_code EC;
        llvm::raw_fd_ostream dest(output_filename, EC, llvm::sys::fs::OF_None);

        if (EC)
        {
            log_error("Could not open file '" + output_filename + "': " + EC.message());
            return;
        }

        llvm::legacy::PassManager pass;
        auto FileType = llvm::CodeGenFileType::ObjectFile;

        if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType))
        {
            log_error("TargetMachine can't emit a file of this type.");
            return;
        }

        pass.run(*llvmModule);
        dest.flush();
    }

    llvm::Type *ScriptCompiler::getMyceliumStringPtrTy()
    {
        if (!myceliumStringType)
            log_error("MyceliumString LLVM type not initialized before getMyceliumStringPtrTy call.");
        return llvm::PointerType::getUnqual(*llvmContext);
    }

    llvm::Type *ScriptCompiler::getMyceliumObjectHeaderPtrTy()
    {
        if (!myceliumObjectHeaderType)
            log_error("MyceliumObjectHeader LLVM type not initialized before getMyceliumObjectHeaderPtrTy call.");
        return llvm::PointerType::getUnqual(*llvmContext);
    }

    void ScriptCompiler::declare_all_runtime_functions()
    {
        if (!llvmModule)
        {
            log_error("LLVM module not available for declaring runtime functions.");
            return;
        }

        // Get the canonical pointer types from the helper methods. This ensures consistency.
        llvm::Type *string_ptr_type = getMyceliumStringPtrTy();
        llvm::Type *header_ptr_type = getMyceliumObjectHeaderPtrTy();

        for (const auto &binding : get_runtime_bindings())
        {
            // Pass the canonical types to the type getter for each binding.
            llvm::FunctionType *func_type = binding.get_llvm_type(*llvmContext, string_ptr_type, header_ptr_type);
            if (!func_type)
            {
                log_error("Failed to get LLVM FunctionType for: " + binding.ir_function_name);
                continue;
            }
            
            // Ensure we don't re-declare a function that might already exist from an extern declaration.
            if (!llvmModule->getFunction(binding.ir_function_name)) {
                llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, binding.ir_function_name, llvmModule.get());
            }
        }
    }

    void ScriptCompiler::populate_class_registry_from_semantic_ir()
    {
        if (!symbolTable)
        {
            log_error("Cannot populate class registry: symbolTable is null");
            return;
        }

        // Get all classes from the SemanticIR and add them to classTypeRegistry
        const auto &classes = symbolTable->get_classes();
        for (const auto &[class_name, class_symbol] : classes)
        {
            // Copy the ClassTypeInfo from the semantic analysis
            ClassTypeInfo cti = class_symbol.type_info;
            cti.name = class_name;
            cti.type_id = next_type_id++;
            classTypeRegistry[class_name] = cti;
        }
    }

    llvm::AllocaInst *ScriptCompiler::create_entry_block_alloca(llvm::Function *function, const std::string &var_name, llvm::Type *type)
    {
        if (!function)
            log_error("Cannot create alloca: currentFunction is null.");
        if (function->empty())
        { // Ensure entry block exists
            llvm::BasicBlock::Create(*llvmContext, "entry.init", function);
        }
        llvm::IRBuilder<> TmpB(&function->getEntryBlock(), function->getEntryBlock().getFirstInsertionPt());
        return TmpB.CreateAlloca(type, nullptr, var_name.c_str());
    }

    llvm::Value *ScriptCompiler::getFieldsPtrFromHeaderPtr(llvm::Value *headerPtr, llvm::StructType *fieldsLLVMStructType)
    {
        if (!myceliumObjectHeaderType)
        {
            log_error("MyceliumObjectHeader type not initialized.");
            return nullptr;
        }
        llvm::Type *i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext));
        llvm::Value *header_as_i8_ptr = llvmBuilder->CreateBitCast(headerPtr, i8PtrTy, "header.i8ptr.forfields");
        uint64_t header_size = llvmModule->getDataLayout().getTypeAllocSize(myceliumObjectHeaderType);
        llvm::Value *offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), header_size);
        llvm::Value *fields_raw_ptr = llvmBuilder->CreateGEP(llvm::Type::getInt8Ty(*llvmContext), header_as_i8_ptr, offset, "fields.rawptr.fromhdr");
        llvm::Value *result_fields_ptr = llvmBuilder->CreateBitCast(fields_raw_ptr, llvm::PointerType::getUnqual(*llvmContext), "fields.ptr.fromhdr");

        std::string debug_msg = "getFieldsPtrFromHeaderPtr:\n  HeaderSize: " + std::to_string(header_size) + "\n  Offset: i64 " + std::to_string(header_size);
        Mycelium::Scripting::Common::LOG_DEBUG(debug_msg, "COMPILER");

        return result_fields_ptr;
    }

    llvm::Value *ScriptCompiler::getHeaderPtrFromFieldsPtr(llvm::Value *fieldsPtr, llvm::StructType *fieldsLLVMStructType)
    {
        if (!myceliumObjectHeaderType)
        {
            log_error("MyceliumObjectHeader type not initialized.");
            return nullptr;
        }
        llvm::Type *i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext));
        llvm::Value *fields_as_i8_ptr = llvmBuilder->CreateBitCast(fieldsPtr, i8PtrTy, "fields.i8ptr.forhdr");
        uint64_t header_size = llvmModule->getDataLayout().getTypeAllocSize(myceliumObjectHeaderType);
        llvm::Value *offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), -static_cast<int64_t>(header_size));
        llvm::Value *header_raw_ptr = llvmBuilder->CreateGEP(llvm::Type::getInt8Ty(*llvmContext), fields_as_i8_ptr, offset, "header.rawptr.fromfields");
        llvm::Value *result_header_ptr = llvmBuilder->CreateBitCast(header_raw_ptr, llvm::PointerType::getUnqual(*llvmContext), "header.ptr.fromfields");

        std::string debug_msg = "getHeaderPtrFromFieldsPtr:\n  HeaderSize: " + std::to_string(header_size) + "\n  Offset: i64 -" + std::to_string(header_size);
        Mycelium::Scripting::Common::LOG_DEBUG(debug_msg, "COMPILER");

        return result_header_ptr;
    }

    llvm::Type *ScriptCompiler::get_llvm_type(std::shared_ptr<TypeNameNode> type_node)
    {
        if (!type_node)
        {
            log_error("TypeNameNode is null when trying to get LLVM type.");
            return nullptr;
        }

        // Simplified: assumes name_segment is IdentifierNode for primitive/class names
        if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&type_node->name_segment))
        {
            if (auto identNode = *identNode_variant)
            {
                return get_llvm_type_from_string(identNode->name, type_node->location);
            }
        }
        else if (auto qnNode_variant = std::get_if<std::shared_ptr<QualifiedNameNode>>(&type_node->name_segment))
        {
            // TODO: Handle qualified names properly. For now, log error or try to extract rightmost.
            // This requires resolving the qualified name to a simple class name string.
            // For now, let's assume qualified names are not directly mapped to simple LLVM types here
            // unless they resolve to a registered class.
            // As a placeholder, try to get the rightmost identifier.
            if (auto qnNode = *qnNode_variant)
            {
                if (qnNode->right)
                {
                    return get_llvm_type_from_string(qnNode->right->name, qnNode->right->location);
                }
            }
        }
        log_error("Unsupported TypeNameNode structure for LLVM type conversion.", type_node->location);
        return nullptr;
    }

    llvm::Type *ScriptCompiler::get_llvm_type_from_string(const std::string &type_name, std::optional<SourceLocation> loc)
    {
        if (type_name == "int" || type_name == "i32")
            return llvm::Type::getInt32Ty(*llvmContext);
        if (type_name == "uint" || type_name == "u32")
            return llvm::Type::getInt32Ty(*llvmContext); // Treat uint as int32 for simplicity
        if (type_name == "bool")
            return llvm::Type::getInt1Ty(*llvmContext);
        if (type_name == "void")
            return llvm::Type::getVoidTy(*llvmContext);
        if (type_name == "float")
            return llvm::Type::getFloatTy(*llvmContext);
        if (type_name == "double")
            return llvm::Type::getDoubleTy(*llvmContext);
        if (type_name == "char" || type_name == "i8")
            return llvm::Type::getInt8Ty(*llvmContext);
        if (type_name == "long" || type_name == "i64")
            return llvm::Type::getInt64Ty(*llvmContext);
        if (type_name == "ulong" || type_name == "u64")
            return llvm::Type::getInt64Ty(*llvmContext);

        if (type_name == "string" || classTypeRegistry.count(type_name))
        {
            return llvm::PointerType::getUnqual(*llvmContext);
        }

        log_error("Unknown type name: '" + type_name + "' encountered during LLVM type lookup.", loc);
        return nullptr;
    }

    std::string ScriptCompiler::get_primitive_name_from_llvm_type(llvm::Type *type)
    {
        // COMPLETELY DISABLE LLVM type introspection to avoid JIT crashes
        // We'll rely entirely on declared type information from variables
        return "";
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::handle_primitive_method_call(
        std::shared_ptr<MethodCallExpressionNode> node,
        PrimitiveStructInfo *primitive_info,
        llvm::Value *instance_ptr)
    {

        if (!primitive_info)
        {
            log_error("Primitive info is null in handle_primitive_method_call.", node->location);
            return ExpressionVisitResult(nullptr);
        }

        // Extract method name from the call
        std::string method_name;
        if (auto memberAccess = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target))
        {
            method_name = memberAccess->memberName->name;
        }
        else
        {
            log_error("Invalid method call structure for primitive method.", node->location);
            return ExpressionVisitResult(nullptr);
        }

        // Handle built-in primitive methods directly here rather than trying to call LLVM functions
        // This provides better performance and simpler implementation

        if (primitive_info->simple_name == "int")
        {
            if (method_name == "ToString")
            {
                // Convert int to string
                llvm::Function *fromIntFunc = llvmModule->getFunction("Mycelium_String_from_int");
                if (!fromIntFunc)
                {
                    log_error("Mycelium_String_from_int not found for int.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result = llvmBuilder->CreateCall(fromIntFunc, {instance_ptr}, "int_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
            else if (method_name == "Parse" && instance_ptr == nullptr)
            { // Static method
                // Parse string to int
                if (!node->argumentList || node->argumentList->arguments.empty())
                {
                    log_error("int.Parse requires a string argument", node->location);
                    return ExpressionVisitResult(nullptr);
                }

                auto arg_res = visit(node->argumentList->arguments[0]->expression);
                if (!arg_res.value)
                {
                    log_error("Failed to compile argument for int.Parse", node->location);
                    return ExpressionVisitResult(nullptr);
                }

                llvm::Function *toIntFunc = llvmModule->getFunction("Mycelium_String_to_int");
                if (!toIntFunc)
                {
                    log_error("Mycelium_String_to_int not found for int.Parse()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result = llvmBuilder->CreateCall(toIntFunc, {arg_res.value}, "parse_int");
                return ExpressionVisitResult(result, nullptr); // Returns int - will be handled as primitive
            }
        }
        else if (primitive_info->simple_name == "bool")
        {
            if (method_name == "ToString")
            {
                // Convert bool to string
                llvm::Function *fromBoolFunc = llvmModule->getFunction("Mycelium_String_from_bool");
                if (!fromBoolFunc)
                {
                    log_error("Mycelium_String_from_bool not found for bool.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result = llvmBuilder->CreateCall(fromBoolFunc, {instance_ptr}, "bool_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }
        else if (primitive_info->simple_name == "string")
        {
            if (method_name == "get_Length")
            {
                // Get string length - returns an int that should support chaining
                llvm::Function *lenFunc = llvmModule->getFunction("Mycelium_String_get_length");
                if (!lenFunc)
                {
                    // Create the function declaration if it doesn't exist
                    llvm::Type *i32Type = llvm::Type::getInt32Ty(*llvmContext);
                    llvm::Type *stringPtrType = getMyceliumStringPtrTy();
                    llvm::FunctionType *lenFuncType = llvm::FunctionType::get(i32Type, {stringPtrType}, false);
                    lenFunc = llvm::Function::Create(lenFuncType, llvm::Function::ExternalLinkage, "Mycelium_String_get_length", llvmModule.get());
                }
                llvm::Value *result = llvmBuilder->CreateCall(lenFunc, {instance_ptr}, "string_length");

                // Return the result with primitive info so method chaining works
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("int");
                return visit_result;
            }
            else if (method_name == "Substring")
            {
                // String substring
                if (!node->argumentList || node->argumentList->arguments.empty())
                {
                    log_error("string.Substring requires a start index argument", node->location);
                    return ExpressionVisitResult(nullptr);
                }

                auto start_res = visit(node->argumentList->arguments[0]->expression);
                if (!start_res.value)
                {
                    log_error("Failed to compile start index for string.Substring", node->location);
                    return ExpressionVisitResult(nullptr);
                }

                llvm::Function *substrFunc = llvmModule->getFunction("Mycelium_String_substring");
                if (!substrFunc)
                {
                    // Create the function declaration
                    llvm::Type *stringPtrType = getMyceliumStringPtrTy();
                    llvm::Type *i32Type = llvm::Type::getInt32Ty(*llvmContext);
                    llvm::FunctionType *substrFuncType = llvm::FunctionType::get(stringPtrType, {stringPtrType, i32Type}, false);
                    substrFunc = llvm::Function::Create(substrFuncType, llvm::Function::ExternalLinkage, "Mycelium_String_substring", llvmModule.get());
                }
                llvm::Value *result = llvmBuilder->CreateCall(substrFunc, {instance_ptr, start_res.value}, "string_substring");
                return ExpressionVisitResult(result, nullptr);
            }
            else if (method_name == "get_Empty" && instance_ptr == nullptr)
            { // Static method
                // Return empty string
                llvm::Function *emptyFunc = llvmModule->getFunction("Mycelium_String_get_empty");
                if (!emptyFunc)
                {
                    // Create the function declaration
                    llvm::Type *stringPtrType = getMyceliumStringPtrTy();
                    llvm::FunctionType *emptyFuncType = llvm::FunctionType::get(stringPtrType, {}, false);
                    emptyFunc = llvm::Function::Create(emptyFuncType, llvm::Function::ExternalLinkage, "Mycelium_String_get_empty", llvmModule.get());
                }
                llvm::Value *result = llvmBuilder->CreateCall(emptyFunc, {}, "string_empty");
                return ExpressionVisitResult(result, nullptr);
            }
        }
        else if (primitive_info->simple_name == "float")
        {
            if (method_name == "ToString")
            {
                // Convert float to string
                llvm::Function *fromFloatFunc = llvmModule->getFunction("Mycelium_String_from_float");
                if (!fromFloatFunc)
                {
                    log_error("Mycelium_String_from_float not found for float.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result = llvmBuilder->CreateCall(fromFloatFunc, {instance_ptr}, "float_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }
        else if (primitive_info->simple_name == "double")
        {
            if (method_name == "ToString")
            {
                // Convert double to string
                llvm::Function *fromDoubleFunc = llvmModule->getFunction("Mycelium_String_from_double");
                if (!fromDoubleFunc)
                {
                    log_error("Mycelium_String_from_double not found for double.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result = llvmBuilder->CreateCall(fromDoubleFunc, {instance_ptr}, "double_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }
        else if (primitive_info->simple_name == "char")
        {
            if (method_name == "ToString")
            {
                // Convert char to string
                llvm::Function *fromCharFunc = llvmModule->getFunction("Mycelium_String_from_char");
                if (!fromCharFunc)
                {
                    log_error("Mycelium_String_from_char not found for char.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result = llvmBuilder->CreateCall(fromCharFunc, {instance_ptr}, "char_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }
        else if (primitive_info->simple_name == "long")
        {
            if (method_name == "ToString")
            {
                // Convert long to string
                llvm::Function *fromLongFunc = llvmModule->getFunction("Mycelium_String_from_long");
                if (!fromLongFunc)
                {
                    log_error("Mycelium_String_from_long not found for long.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result = llvmBuilder->CreateCall(fromLongFunc, {instance_ptr}, "long_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }

        log_error("Unsupported primitive method: " + primitive_info->simple_name + "." + method_name, node->location);
        return ExpressionVisitResult(nullptr);
    }

    void ScriptCompiler::declare_class_structure_and_signatures(std::shared_ptr<ClassDeclarationNode> node, const std::string &fq_class_name)
    {
        if (classTypeRegistry.count(fq_class_name))
        {
            // Already processed this class structure, skip it
            return;
        }

        // Look up the class in the SemanticIR to get all its information
        ClassTypeInfo cti;
        if (symbolTable)
        {
            const auto *class_symbol = symbolTable->find_class(fq_class_name);
            if (class_symbol)
            {
                // Use the ClassTypeInfo from the semantic analysis
                cti = class_symbol->type_info;
                cti.name = fq_class_name;
                cti.type_id = next_type_id++;
            }
            else
            {
                log_error("Class not found in SemanticIR: " + fq_class_name, node->location);
                return;
            }
        }
        else
        {
            // Fallback to old method if SemanticIR not available
            cti.name = fq_class_name;
            cti.type_id = next_type_id++;
        }

        // Create LLVM struct type for fields with inheritance support
        std::vector<llvm::Type *> field_llvm_types_for_struct;
        unsigned field_idx_counter = 0;
        
        // First, add base class fields if there's inheritance
        if (symbolTable)
        {
            const auto *class_symbol = symbolTable->find_class(fq_class_name);
            if (class_symbol && !class_symbol->base_class.empty())
            {
                // Find the base class in our registry
                auto base_class_it = classTypeRegistry.find(class_symbol->base_class);
                if (base_class_it != classTypeRegistry.end())
                {
                    const ClassTypeInfo& base_cti = base_class_it->second;
                    
                    // Flatten base class fields directly into derived class (no nesting)
                    if (base_cti.fieldsType)
                    {
                        // Add each base class field type individually (flattened approach)
                        for (size_t i = 0; i < base_cti.field_names_in_order.size(); ++i)
                        {
                            const std::string& base_field_name = base_cti.field_names_in_order[i];
                            llvm::Type* base_field_type = base_cti.fieldsType->getElementType(i);
                            
                            // Add the field type to the derived class struct
                            field_llvm_types_for_struct.push_back(base_field_type);
                            
                            // Add field with both original name (for direct access) and qualified name
                            cti.field_names_in_order.push_back(base_field_name);
                            cti.field_indices[base_field_name] = field_idx_counter;  // Direct access
                            cti.field_indices["base." + base_field_name] = field_idx_counter;  // Qualified access
                            field_idx_counter++;
                        }
                        
                        // Copy base class field AST types
                        for (const auto& base_field_ast_type : base_cti.field_ast_types)
                        {
                            cti.field_ast_types.push_back(base_field_ast_type);
                        }
                        
                        LOG_DEBUG("Added flattened base class fields from " + class_symbol->base_class + " to " + fq_class_name, "COMPILER");
                    }
                }
                else
                {
                    log_error("Base class not found in registry: " + class_symbol->base_class + " for class " + fq_class_name, node->location);
                }
            }
        }
        
        // Then, add derived class's own fields
        for (const auto &member : node->members)
        {
            if (auto fieldDecl = std::dynamic_pointer_cast<FieldDeclarationNode>(member))
            {
                if (!fieldDecl->type)
                {
                    log_error("Field missing type in " + fq_class_name, fieldDecl->location);
                    continue;
                }
                llvm::Type *actual_field_llvm_type = get_llvm_type(fieldDecl->type.value());
                if (!actual_field_llvm_type)
                {
                    log_error("Could not get LLVM type for field in " + fq_class_name, fieldDecl->type.value()->location);
                    continue;
                }
                for (const auto &declarator : fieldDecl->declarators)
                {
                    field_llvm_types_for_struct.push_back(actual_field_llvm_type);
                    cti.field_names_in_order.push_back(declarator->name->name);
                    cti.field_indices[declarator->name->name] = field_idx_counter++;
                    cti.field_ast_types.push_back(fieldDecl->type.value());
                }
            }
        }

        cti.fieldsType = llvm::StructType::create(*llvmContext, field_llvm_types_for_struct, fq_class_name + "_Fields");
        if (!cti.fieldsType)
        {
            log_error("Failed to create fields struct for " + fq_class_name, node->location);
            return;
        }

        // Generate VTable for classes with virtual methods
        if (symbolTable)
        {
            const auto *class_symbol = symbolTable->find_class(fq_class_name);
            if (class_symbol && !class_symbol->virtual_method_order.empty())
            {
                generate_vtable_for_class(cti, class_symbol, fq_class_name);
            }
        }

        classTypeRegistry[fq_class_name] = cti;

        // Declare all method signatures for this class
        for (const auto &member : node->members)
        {
            if (auto methodDecl = std::dynamic_pointer_cast<MethodDeclarationNode>(member))
            {
                declare_method_signature(methodDecl, fq_class_name);
            }
            else if (auto ctorDecl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member))
            {
                declare_constructor_signature(ctorDecl, fq_class_name);
            }
            else if (auto dtorDecl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member))
            {
                declare_destructor_signature(dtorDecl, fq_class_name);
            }
        }
    }

    void ScriptCompiler::generate_vtable_for_class(ClassTypeInfo& cti, const SymbolTable::ClassSymbol* class_symbol, const std::string& fq_class_name)
    {
        LOG_DEBUG("Generating VTable for class: " + fq_class_name, "COMPILER");
        
        // Create VTable type - first slot is destructor, then virtual methods
        std::vector<llvm::Type*> vtable_slot_types;
        
        // Slot 0: Destructor function pointer (always present for ARC cleanup)
        // Signature: void(ptr) - takes object header pointer
        llvm::Type* void_type = llvm::Type::getVoidTy(*llvmContext);
        llvm::Type* ptr_type = llvm::PointerType::get(*llvmContext, 0);
        llvm::FunctionType* destructor_type = llvm::FunctionType::get(void_type, {ptr_type}, false);
        vtable_slot_types.push_back(llvm::PointerType::get(destructor_type, 0));
        
        // Add slots for each virtual method in order
        for (const std::string& virtual_method_qualified_name : class_symbol->virtual_method_order)
        {
            // Find the method in the global method registry
            const auto* method_symbol = symbolTable->find_method(virtual_method_qualified_name);
            if (!method_symbol)
            {
                log_error("Virtual method not found in symbol table: " + virtual_method_qualified_name, std::nullopt);
                continue;
            }
            
            // Generate LLVM function signature for this virtual method
            std::vector<llvm::Type*> param_types;
            
            // First parameter is always 'this' pointer (object header ptr)
            param_types.push_back(ptr_type);
            
            // Add method parameters
            for (const auto& param_type_node : method_symbol->parameter_types)
            {
                llvm::Type* param_llvm_type = get_llvm_type(param_type_node);
                if (param_llvm_type)
                {
                    param_types.push_back(param_llvm_type);
                }
            }
            
            // Get return type
            llvm::Type* return_type = get_llvm_type(method_symbol->return_type);
            if (!return_type)
            {
                return_type = void_type; // Default to void if type not found
            }
            
            // Create function type and add pointer to VTable
            llvm::FunctionType* method_func_type = llvm::FunctionType::get(return_type, param_types, false);
            vtable_slot_types.push_back(llvm::PointerType::get(method_func_type, 0));
        }
        
        // Create the VTable struct type
        std::string vtable_type_name = fq_class_name + "_VTable";
        cti.vtable_type = llvm::StructType::create(*llvmContext, vtable_slot_types, vtable_type_name);
        
        // Create global VTable constant (will be populated later with actual function pointers)
        std::string vtable_global_name = fq_class_name + "_vtable_global";
        cti.vtable_global = new llvm::GlobalVariable(
            *llvmModule,
            cti.vtable_type,
            true, // isConstant
            llvm::GlobalValue::ExternalLinkage,
            nullptr, // initializer will be set later
            vtable_global_name
        );
        
        LOG_INFO("Generated VTable for " + fq_class_name + " with " + 
                 std::to_string(class_symbol->virtual_method_order.size()) + " virtual methods", "COMPILER");
    }

    void ScriptCompiler::populate_vtable_for_class(const std::string& fq_class_name)
    {
        // Find the class in our registry
        auto class_it = classTypeRegistry.find(fq_class_name);
        if (class_it == classTypeRegistry.end())
        {
            return; // Class not found or not processed
        }
        
        ClassTypeInfo& cti = class_it->second;
        if (!cti.vtable_global || !cti.vtable_type)
        {
            return; // No VTable for this class
        }
        
        const auto* class_symbol = symbolTable->find_class(fq_class_name);
        if (!class_symbol)
        {
            log_error("Class symbol not found when populating VTable: " + fq_class_name, std::nullopt);
            return;
        }
        
        LOG_DEBUG("Populating VTable for class: " + fq_class_name, "COMPILER");
        
        std::vector<llvm::Constant*> vtable_initializers;
        
        // Slot 0: Destructor function pointer
        // For now, use a null pointer placeholder - destructor handling will be enhanced later
        llvm::Type* destructor_ptr_type = cti.vtable_type->getElementType(0);
        vtable_initializers.push_back(llvm::ConstantPointerNull::get(
            llvm::cast<llvm::PointerType>(destructor_ptr_type)
        ));
        
        // Populate virtual method slots
        for (size_t i = 0; i < class_symbol->virtual_method_order.size(); ++i)
        {
            const std::string& virtual_method_qualified_name = class_symbol->virtual_method_order[i];
            
            // Find the actual LLVM function for this method
            llvm::Function* method_func = llvmModule->getFunction(virtual_method_qualified_name);
            if (!method_func)
            {
                log_error("Virtual method function not found: " + virtual_method_qualified_name, std::nullopt);
                // Use null pointer as fallback
                llvm::Type* method_ptr_type = cti.vtable_type->getElementType(i + 1); // +1 for destructor slot
                vtable_initializers.push_back(llvm::ConstantPointerNull::get(
                    llvm::cast<llvm::PointerType>(method_ptr_type)
                ));
                continue;
            }
            
            // Add function pointer to VTable
            vtable_initializers.push_back(method_func);
        }
        
        // Create the VTable initializer
        llvm::Constant* vtable_initializer = llvm::ConstantStruct::get(cti.vtable_type, vtable_initializers);
        
        // Set the initializer for the global VTable
        cti.vtable_global->setInitializer(vtable_initializer);
        
        LOG_INFO("Populated VTable for " + fq_class_name + " with " + 
                 std::to_string(class_symbol->virtual_method_order.size()) + " virtual methods", "COMPILER");
    }

    void ScriptCompiler::compile_all_method_bodies(std::shared_ptr<ClassDeclarationNode> node, const std::string& fq_class_name)
    {
        // Compile method bodies (now all signatures across all classes are available)
        for (const auto &member : node->members)
        {
            if (auto methodDecl = std::dynamic_pointer_cast<MethodDeclarationNode>(member))
            {
                compile_method_body(methodDecl, fq_class_name);
            }
            else if (auto ctorDecl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member))
            {
                compile_constructor_body(ctorDecl, fq_class_name);
            }
            else if (auto dtorDecl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member))
            {
                compile_destructor_body(dtorDecl, fq_class_name);
            }
        }
    }

    // The visit methods will be moved to separate files (compiler_expressions.cpp, etc.)

} // namespace Mycelium::Scripting::Lang
