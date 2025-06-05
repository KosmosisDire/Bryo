#include "sharpie/compiler/script_compiler.hpp"
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
#include "llvm/IR/IRBuilder.h" // Used by create_entry_block_alloca
#include "llvm/Support/CodeGen.h" // For CodeGenFileType

// Assuming runtime_binding.h is in a path accessible to the compiler, e.g., lib/
// If it's relative to this file, the path needs adjustment.
#include "../../lib/runtime_binding.h" // Corrected path

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm> // For std::find_if

namespace Mycelium::Scripting::Lang
{
    // Initialize static members
    bool ScriptCompiler::jit_initialized = false;
    bool ScriptCompiler::aot_initialized = false;

    ScriptCompiler::ScriptCompiler()
        : next_type_id(0) // Initialize next_type_id
    {
        llvmContext = std::make_unique<llvm::LLVMContext>();
        // llvmModule and llvmBuilder are initialized in compile_ast
        
        // Initialize primitive struct registry
        primitive_registry.initialize_builtin_primitives();
    }

    ScriptCompiler::~ScriptCompiler() = default;

    void ScriptCompiler::compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string &module_name)
    {
        if (!llvmContext) // Should have been created by constructor
        {
            llvmContext = std::make_unique<llvm::LLVMContext>();
        }
        llvmModule = std::make_unique<llvm::Module>(module_name, *llvmContext);
        llvmBuilder = std::make_unique<llvm::IRBuilder<>>(*llvmContext);

        // Initialize MyceliumString type
        // Ensure this is done only once or correctly re-initialized if needed.
        // For now, assuming it's okay to define here if not already defined.
        if (!myceliumStringType) 
        {
            myceliumStringType = llvm::StructType::create(*llvmContext,
                                                          {llvm::PointerType::getUnqual(*llvmContext), // char* data
                                                           llvm::Type::getInt64Ty(*llvmContext),      // length
                                                           llvm::Type::getInt64Ty(*llvmContext)},     // capacity
                                                          "struct.MyceliumString");
            if (!myceliumStringType) log_error("Failed to initialize MyceliumString LLVM type struct.");
        }
        
        // Initialize MyceliumObjectHeader type
        if (!myceliumObjectHeaderType)
        {
             myceliumObjectHeaderType = llvm::StructType::create(*llvmContext,
                                                                {llvm::Type::getInt32Ty(*llvmContext),      // ref_count (int32_t)
                                                                 llvm::Type::getInt32Ty(*llvmContext),      // type_id (uint32_t, treated as i32)
                                                                 llvm::PointerType::getUnqual(*llvmContext)}, // vtable (MyceliumVTable*)
                                                                "struct.MyceliumObjectHeader");
            if (!myceliumObjectHeaderType) log_error("Failed to initialize MyceliumObjectHeader LLVM type struct.");
        }


        declare_all_runtime_functions(); // Declare C runtime functions in the LLVM module

        namedValues.clear();
        currentFunction = nullptr;
        classTypeRegistry.clear(); // Clear previous class type info
        functionReturnClassInfoMap.clear(); // Clear function return type map
        next_type_id = 0;          // Reset type ID counter

        if (!ast_root) {
            log_error("AST root is null. Cannot compile.");
            // No throw here, allow verifyModule to catch issues if it proceeds.
            return; 
        }
        
        visit(ast_root); // This will call the main visit(CompilationUnitNode)

        // Verify the module for errors
        if (llvm::verifyModule(*llvmModule, &llvm::errs()))
        {
            log_error("LLVM Module verification failed. Dumping potentially corrupt IR.");
            // llvmModule->print(llvm::errs(), nullptr); // Already done by log_error essentially
            throw std::runtime_error("LLVM module verification failed.");
        }
    }

    std::string ScriptCompiler::get_ir_string() const
    {
        if (!llvmModule) return "[No LLVM Module Initialized]";
        std::string irString;
        llvm::raw_string_ostream ostream(irString);
        llvmModule->print(ostream, nullptr);
        ostream.flush();
        return irString;
    }

    void ScriptCompiler::dump_ir() const
    {
        if (!llvmModule) { llvm::errs() << "[No LLVM Module Initialized to dump]\n"; return; }
        llvmModule->print(llvm::errs(), nullptr);
    }
    
    std::unique_ptr<llvm::Module> ScriptCompiler::take_module()
    {
        if (!llvmModule) {
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
        if (loc && loc->fileName != "unknown") { // Check if location is meaningful
             ss << " " << loc->to_string();
        }
        ss << ": " << message;
        llvm::errs() << ss.str() << "\n";
        if (llvmModule) { // Dump IR if module exists, might help debug
            llvm::errs() << "Current LLVM IR state:\n";
            llvmModule->print(llvm::errs(), nullptr);
        }
        throw std::runtime_error(ss.str());
    }

    std::string ScriptCompiler::llvm_type_to_string(llvm::Type *type) const
    {
        if (!type) return "<null llvm type>";
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

        if (!llvmModule) { // Check if llvmModule was moved or not created
            log_error("No LLVM module available for JIT. Was it compiled or taken?");
        }
        
        // Create a new module for the JIT if the main one is gone, or clone.
        // For simplicity, let's assume we take_module for JIT.
        std::unique_ptr<llvm::Module> jitModule = take_module();
        if (!jitModule) {
             log_error("Failed to obtain LLVM module for JIT execution.");
        }

        std::string errStr;
        llvm::EngineBuilder engineBuilder(std::move(jitModule)); // Pass ownership
        engineBuilder.setErrorStr(&errStr);
        engineBuilder.setEngineKind(llvm::EngineKind::JIT); 
        engineBuilder.setVerifyModules(true); 
        engineBuilder.setOptLevel(llvm::CodeGenOptLevel::None); 
        
        llvm::ExecutionEngine *ee = engineBuilder.create();

        if (!ee) {
            log_error("Failed to construct ExecutionEngine: " + errStr);
        }

        // Add global mappings for runtime functions
        for (const auto &binding : get_runtime_bindings()) {
            if (binding.c_function_pointer == nullptr) {
                // llvm::errs() << "Skipping null C function pointer for: " << binding.ir_function_name << "\n";
                continue;
            }
            // llvm::errs() << "Mapping IR func: " << binding.ir_function_name << " to C func at " << binding.c_function_pointer << "\n";
            ee->addGlobalMapping(binding.ir_function_name, reinterpret_cast<uint64_t>(binding.c_function_pointer));
        }
        
        ee->finalizeObject(); // Finalize object code before running.
        
        llvm::Function *funcToRun = ee->FindFunctionNamed(function_name);
        if (!funcToRun) {
            delete ee; // Clean up ExecutionEngine
            log_error("JIT: Function '" + function_name + "' not found in the module.");
        }
        
        llvm::GenericValue result; 
        try {
            result = ee->runFunction(funcToRun, args);
        } catch (const std::exception& e) {
            delete ee;
            log_error("JIT: Exception during runFunction: " + std::string(e.what()));
        } catch (...) {
            delete ee;
            log_error("JIT: Unknown exception during runFunction.");
        }
        
        delete ee; // Clean up ExecutionEngine
        return result;
    }

    void ScriptCompiler::initialize_aot_engine_dependencies() { 
        if(!aot_initialized) {
            // Initialize all targets and components for AOT compilation
            llvm::InitializeAllTargets();
            llvm::InitializeAllTargetMCs();
            llvm::InitializeAllAsmPrinters();
            llvm::InitializeAllAsmParsers();
            aot_initialized = true;
        }
    }

    void ScriptCompiler::compile_to_object_file(const std::string &output_filename) {
        initialize_aot_engine_dependencies();

        if (!llvmModule) {
            log_error("No LLVM module available for AOT compilation to object file.");
            return;
        }

        auto TargetTriple = llvm::sys::getDefaultTargetTriple();
        llvmModule->setTargetTriple(TargetTriple);

        std::string Error;
        auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

        if (!Target) {
            log_error("Failed to lookup target: " + Error);
            return;
        }

        auto CPU = llvm::sys::getHostCPUName().str(); 
        
        llvm::StringMap<bool> HostFeatures;
        std::string FeaturesStr;
        if (llvm::sys::getHostCPUFeatures(HostFeatures)) { 
            llvm::raw_string_ostream OS(FeaturesStr);
            bool first = true;
            for (auto &Feature : HostFeatures) {
                if (!first) { OS << ","; }
                first = false;
                OS << (Feature.getValue() ? "+" : "-") << Feature.getKey();
            }
            OS.flush();
        }
        
        llvm::TargetOptions opt;
        llvm::Reloc::Model RM = llvm::Reloc::Static; 
        auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, FeaturesStr, opt, RM);

        if (!TheTargetMachine) {
            log_error("Could not create target machine for " + TargetTriple);
            return;
        }

        llvmModule->setDataLayout(TheTargetMachine->createDataLayout());

        std::error_code EC;
        llvm::raw_fd_ostream dest(output_filename, EC, llvm::sys::fs::OF_None);

        if (EC) {
            log_error("Could not open file '" + output_filename + "': " + EC.message());
            return;
        }

        llvm::legacy::PassManager pass;
        auto FileType = llvm::CodeGenFileType::ObjectFile; 

        if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
            log_error("TargetMachine can't emit a file of this type.");
            return;
        }

        pass.run(*llvmModule);
        dest.flush();
    }

    llvm::Type* ScriptCompiler::getMyceliumStringPtrTy() {
        if (!myceliumStringType) log_error("MyceliumString LLVM type not initialized before getMyceliumStringPtrTy call.");
        return llvm::PointerType::getUnqual(*llvmContext); 
    }

    llvm::Type* ScriptCompiler::getMyceliumObjectHeaderPtrTy() {
        if (!myceliumObjectHeaderType) log_error("MyceliumObjectHeader LLVM type not initialized before getMyceliumObjectHeaderPtrTy call.");
        return llvm::PointerType::getUnqual(*llvmContext);
    }
    
    void ScriptCompiler::declare_all_runtime_functions() { 
        if (!llvmModule) { log_error("LLVM module not available for declaring runtime functions."); return; }
        llvm::Type* opaque_ptr_type = llvm::PointerType::getUnqual(*llvmContext); // General opaque pointer
        llvm::Type* i64_type = llvm::Type::getInt64Ty(*llvmContext);
        llvm::Type* i32_type = llvm::Type::getInt32Ty(*llvmContext);
        llvm::Type* void_type = llvm::Type::getVoidTy(*llvmContext);
        llvm::Type* i8_ptr_type = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext));


        for (const auto &binding : get_runtime_bindings()) {
            llvm::FunctionType *func_type = binding.get_llvm_type(*llvmContext, opaque_ptr_type, i8_ptr_type);
            if (!func_type) { log_error("Failed to get LLVM FunctionType for: " + binding.ir_function_name); continue; }
            llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, binding.ir_function_name, llvmModule.get());
        }
    }
    
    llvm::AllocaInst *ScriptCompiler::create_entry_block_alloca(llvm::Function *function, const std::string &var_name, llvm::Type *type) {
        if (!function) log_error("Cannot create alloca: currentFunction is null.");
        if (function->empty()) { // Ensure entry block exists
             llvm::BasicBlock::Create(*llvmContext, "entry.init", function);
        }
        llvm::IRBuilder<> TmpB(&function->getEntryBlock(), function->getEntryBlock().getFirstInsertionPt());
        return TmpB.CreateAlloca(type, nullptr, var_name.c_str());
    }

    llvm::Value* ScriptCompiler::getFieldsPtrFromHeaderPtr(llvm::Value* headerPtr, llvm::StructType* fieldsLLVMStructType) {
        if (!myceliumObjectHeaderType) { log_error("MyceliumObjectHeader type not initialized."); return nullptr; }
        llvm::Type* i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext)); 
    llvm::Value* header_as_i8_ptr = llvmBuilder->CreateBitCast(headerPtr, i8PtrTy, "header.i8ptr.forfields");
    uint64_t header_size = llvmModule->getDataLayout().getTypeAllocSize(myceliumObjectHeaderType);
    llvm::Value* offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), header_size);
    llvm::Value* fields_raw_ptr = llvmBuilder->CreateGEP(llvm::Type::getInt8Ty(*llvmContext), header_as_i8_ptr, offset, "fields.rawptr.fromhdr");
    llvm::Value* result_fields_ptr = llvmBuilder->CreateBitCast(fields_raw_ptr, llvm::PointerType::getUnqual(*llvmContext), "fields.ptr.fromhdr");

    llvm::errs() << "getFieldsPtrFromHeaderPtr:\n";
    llvm::errs() << "  HeaderIn: "; headerPtr->print(llvm::errs()); llvm::errs() << "\n";
    llvm::errs() << "  HeaderSize: " << header_size << "\n";
    llvm::errs() << "  Offset: "; offset->print(llvm::errs()); llvm::errs() << "\n";
    llvm::errs() << "  FieldsOut: "; result_fields_ptr->print(llvm::errs()); llvm::errs() << "\n";

    return result_fields_ptr;
}

llvm::Value* ScriptCompiler::getHeaderPtrFromFieldsPtr(llvm::Value* fieldsPtr, llvm::StructType* fieldsLLVMStructType) {
    if (!myceliumObjectHeaderType) { log_error("MyceliumObjectHeader type not initialized."); return nullptr; }
    llvm::Type* i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext));
    llvm::Value* fields_as_i8_ptr = llvmBuilder->CreateBitCast(fieldsPtr, i8PtrTy, "fields.i8ptr.forhdr");
    uint64_t header_size = llvmModule->getDataLayout().getTypeAllocSize(myceliumObjectHeaderType);
    llvm::Value* offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), -static_cast<int64_t>(header_size));
    llvm::Value* header_raw_ptr = llvmBuilder->CreateGEP(llvm::Type::getInt8Ty(*llvmContext), fields_as_i8_ptr, offset, "header.rawptr.fromfields");
    llvm::Value* result_header_ptr = llvmBuilder->CreateBitCast(header_raw_ptr, llvm::PointerType::getUnqual(*llvmContext), "header.ptr.fromfields");

    llvm::errs() << "getHeaderPtrFromFieldsPtr:\n";
    llvm::errs() << "  FieldsIn: "; fieldsPtr->print(llvm::errs()); llvm::errs() << "\n";
    llvm::errs() << "  HeaderSize: " << header_size << "\n";
    llvm::errs() << "  Offset: "; offset->print(llvm::errs()); llvm::errs() << "\n";
    llvm::errs() << "  HeaderOut: "; result_header_ptr->print(llvm::errs()); llvm::errs() << "\n";
    
    return result_header_ptr;
}

llvm::Type *ScriptCompiler::get_llvm_type(std::shared_ptr<TypeNameNode> type_node) {
        if (!type_node) { log_error("TypeNameNode is null when trying to get LLVM type."); return nullptr; }
        
        // Simplified: assumes name_segment is IdentifierNode for primitive/class names
        if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&type_node->name_segment)) {
            if (auto identNode = *identNode_variant) {
                 return get_llvm_type_from_string(identNode->name, type_node->location);
            }
        } else if (auto qnNode_variant = std::get_if<std::shared_ptr<QualifiedNameNode>>(&type_node->name_segment)) {
            // TODO: Handle qualified names properly. For now, log error or try to extract rightmost.
            // This requires resolving the qualified name to a simple class name string.
            // For now, let's assume qualified names are not directly mapped to simple LLVM types here
            // unless they resolve to a registered class.
            // This part needs a robust name resolution for qualified types.
            // As a placeholder, try to get the rightmost identifier.
            if (auto qnNode = *qnNode_variant) {
                if (qnNode->right) {
                     return get_llvm_type_from_string(qnNode->right->name, qnNode->right->location);
                }
            }
        }
        log_error("Unsupported TypeNameNode structure for LLVM type conversion.", type_node->location);
        return nullptr;
    }

    llvm::Type *ScriptCompiler::get_llvm_type_from_string(const std::string &type_name, std::optional<SourceLocation> loc) {
        if (type_name == "int" || type_name == "i32") return llvm::Type::getInt32Ty(*llvmContext);
        if (type_name == "bool") return llvm::Type::getInt1Ty(*llvmContext);
        if (type_name == "void") return llvm::Type::getVoidTy(*llvmContext);
        if (type_name == "float") return llvm::Type::getFloatTy(*llvmContext);
        if (type_name == "double") return llvm::Type::getDoubleTy(*llvmContext);
        if (type_name == "char" || type_name == "i8") return llvm::Type::getInt8Ty(*llvmContext);
        if (type_name == "long" || type_name == "i64") return llvm::Type::getInt64Ty(*llvmContext);
        
        if (type_name == "string" || classTypeRegistry.count(type_name)) {
            return llvm::PointerType::getUnqual(*llvmContext); 
        }
        
        log_error("Unknown type name: '" + type_name + "' encountered during LLVM type lookup.", loc);
        return nullptr;
    }

    std::string ScriptCompiler::get_primitive_name_from_llvm_type(llvm::Type* type) {
        // COMPLETELY DISABLE LLVM type introspection to avoid JIT crashes
        // We'll rely entirely on declared type information from variables
        return "";
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::handle_primitive_method_call(
        std::shared_ptr<MethodCallExpressionNode> node, 
        PrimitiveStructInfo* primitive_info, 
        llvm::Value* instance_ptr) {
        
        if (!primitive_info) {
            log_error("Primitive info is null in handle_primitive_method_call.", node->location);
            return ExpressionVisitResult(nullptr);
        }

        // Extract method name from the call
        std::string method_name;
        if (auto memberAccess = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target)) {
            method_name = memberAccess->memberName->name;
        } else {
            log_error("Invalid method call structure for primitive method.", node->location);
            return ExpressionVisitResult(nullptr);
        }

        // Handle built-in primitive methods directly here rather than trying to call LLVM functions
        // This provides better performance and simpler implementation
        
        if (primitive_info->simple_name == "int") {
            if (method_name == "ToString") {
                // Convert int to string
                llvm::Function* fromIntFunc = llvmModule->getFunction("Mycelium_String_from_int");
                if (!fromIntFunc) {
                    log_error("Mycelium_String_from_int not found for int.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value* result = llvmBuilder->CreateCall(fromIntFunc, {instance_ptr}, "int_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
            else if (method_name == "Parse" && instance_ptr == nullptr) { // Static method
                // Parse string to int
                if (!node->argumentList || node->argumentList->arguments.empty()) {
                    log_error("int.Parse requires a string argument", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                
                auto arg_res = visit(node->argumentList->arguments[0]->expression);
                if (!arg_res.value) {
                    log_error("Failed to compile argument for int.Parse", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                
                llvm::Function* toIntFunc = llvmModule->getFunction("Mycelium_String_to_int");
                if (!toIntFunc) {
                    log_error("Mycelium_String_to_int not found for int.Parse()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value* result = llvmBuilder->CreateCall(toIntFunc, {arg_res.value}, "parse_int");
                return ExpressionVisitResult(result, nullptr); // Returns int - will be handled as primitive
            }
        }
        else if (primitive_info->simple_name == "bool") {
            if (method_name == "ToString") {
                // Convert bool to string
                llvm::Function* fromBoolFunc = llvmModule->getFunction("Mycelium_String_from_bool");
                if (!fromBoolFunc) {
                    log_error("Mycelium_String_from_bool not found for bool.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value* result = llvmBuilder->CreateCall(fromBoolFunc, {instance_ptr}, "bool_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }
        else if (primitive_info->simple_name == "string") {
            if (method_name == "get_Length") {
                // Get string length - returns an int that should support chaining
                llvm::Function* lenFunc = llvmModule->getFunction("Mycelium_String_get_length");
                if (!lenFunc) {
                    // Create the function declaration if it doesn't exist
                    llvm::Type* i32Type = llvm::Type::getInt32Ty(*llvmContext);
                    llvm::Type* stringPtrType = getMyceliumStringPtrTy();
                    llvm::FunctionType* lenFuncType = llvm::FunctionType::get(i32Type, {stringPtrType}, false);
                    lenFunc = llvm::Function::Create(lenFuncType, llvm::Function::ExternalLinkage, "Mycelium_String_get_length", llvmModule.get());
                }
                llvm::Value* result = llvmBuilder->CreateCall(lenFunc, {instance_ptr}, "string_length");
                
                // Return the result with primitive info so method chaining works
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("int");
                return visit_result;
            }
            else if (method_name == "Substring") {
                // String substring
                if (!node->argumentList || node->argumentList->arguments.empty()) {
                    log_error("string.Substring requires a start index argument", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                
                auto start_res = visit(node->argumentList->arguments[0]->expression);
                if (!start_res.value) {
                    log_error("Failed to compile start index for string.Substring", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                
                llvm::Function* substrFunc = llvmModule->getFunction("Mycelium_String_substring");
                if (!substrFunc) {
                    // Create the function declaration
                    llvm::Type* stringPtrType = getMyceliumStringPtrTy();
                    llvm::Type* i32Type = llvm::Type::getInt32Ty(*llvmContext);
                    llvm::FunctionType* substrFuncType = llvm::FunctionType::get(stringPtrType, {stringPtrType, i32Type}, false);
                    substrFunc = llvm::Function::Create(substrFuncType, llvm::Function::ExternalLinkage, "Mycelium_String_substring", llvmModule.get());
                }
                llvm::Value* result = llvmBuilder->CreateCall(substrFunc, {instance_ptr, start_res.value}, "string_substring");
                return ExpressionVisitResult(result, nullptr);
            }
            else if (method_name == "get_Empty" && instance_ptr == nullptr) { // Static method
                // Return empty string
                llvm::Function* emptyFunc = llvmModule->getFunction("Mycelium_String_get_empty");
                if (!emptyFunc) {
                    // Create the function declaration
                    llvm::Type* stringPtrType = getMyceliumStringPtrTy();
                    llvm::FunctionType* emptyFuncType = llvm::FunctionType::get(stringPtrType, {}, false);
                    emptyFunc = llvm::Function::Create(emptyFuncType, llvm::Function::ExternalLinkage, "Mycelium_String_get_empty", llvmModule.get());
                }
                llvm::Value* result = llvmBuilder->CreateCall(emptyFunc, {}, "string_empty");
                return ExpressionVisitResult(result, nullptr);
            }
        }
        else if (primitive_info->simple_name == "float") {
            if (method_name == "ToString") {
                // Convert float to string
                llvm::Function* fromFloatFunc = llvmModule->getFunction("Mycelium_String_from_float");
                if (!fromFloatFunc) {
                    log_error("Mycelium_String_from_float not found for float.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value* result = llvmBuilder->CreateCall(fromFloatFunc, {instance_ptr}, "float_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }
        else if (primitive_info->simple_name == "double") {
            if (method_name == "ToString") {
                // Convert double to string
                llvm::Function* fromDoubleFunc = llvmModule->getFunction("Mycelium_String_from_double");
                if (!fromDoubleFunc) {
                    log_error("Mycelium_String_from_double not found for double.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value* result = llvmBuilder->CreateCall(fromDoubleFunc, {instance_ptr}, "double_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }
        else if (primitive_info->simple_name == "char") {
            if (method_name == "ToString") {
                // Convert char to string
                llvm::Function* fromCharFunc = llvmModule->getFunction("Mycelium_String_from_char");
                if (!fromCharFunc) {
                    log_error("Mycelium_String_from_char not found for char.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value* result = llvmBuilder->CreateCall(fromCharFunc, {instance_ptr}, "char_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }
        else if (primitive_info->simple_name == "long") {
            if (method_name == "ToString") {
                // Convert long to string
                llvm::Function* fromLongFunc = llvmModule->getFunction("Mycelium_String_from_long");
                if (!fromLongFunc) {
                    log_error("Mycelium_String_from_long not found for long.ToString()", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value* result = llvmBuilder->CreateCall(fromLongFunc, {instance_ptr}, "long_tostring");
                ExpressionVisitResult visit_result(result, nullptr);
                visit_result.primitive_info = primitive_registry.get_by_simple_name("string");
                return visit_result;
            }
        }

        log_error("Unsupported primitive method: " + primitive_info->simple_name + "." + method_name, node->location);
        return ExpressionVisitResult(nullptr);
    }

    // The visit methods will be moved to separate files (compiler_expressions.cpp, etc.)

} // namespace Mycelium::Scripting::Lang
