#include "script_compiler.hpp"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/CFG.h" 
#include "llvm/IR/DerivedTypes.h" 
#include "llvm/Support/Casting.h" 

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
#include "runtime_binding.h" // Assuming this includes necessary C function declarations

#include <iostream>
#include <sstream>
#include <fstream>

namespace Mycelium::Scripting::Lang
{
    // Initialize static members
    bool ScriptCompiler::jit_initialized = false;
    bool ScriptCompiler::aot_initialized = false;

    ScriptCompiler::ScriptCompiler()
        : next_type_id(0)
    {
        llvmContext = std::make_unique<llvm::LLVMContext>();
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

        if (!myceliumStringType)
        {
            myceliumStringType = llvm::StructType::create(*llvmContext,
                                                          {llvm::PointerType::getUnqual(*llvmContext), 
                                                           llvm::Type::getInt64Ty(*llvmContext),      
                                                           llvm::Type::getInt64Ty(*llvmContext)},     
                                                          "struct.MyceliumString");
            if (!myceliumStringType) log_error("Failed to initialize MyceliumString LLVM type struct.");
        }

        if (!myceliumObjectHeaderType)
        {
            myceliumObjectHeaderType = llvm::StructType::create(*llvmContext,
                                                                {llvm::Type::getInt32Ty(*llvmContext),
                                                                 llvm::Type::getInt32Ty(*llvmContext)},
                                                                "struct.MyceliumObjectHeader");
            if (!myceliumObjectHeaderType) log_error("Failed to initialize MyceliumObjectHeader LLVM type struct.");
        }

        declare_all_runtime_functions();
        namedValues.clear();
        currentFunction = nullptr;
        classTypeRegistry.clear(); 
        next_type_id = 0;          

        if (!ast_root) log_error("AST root is null. Cannot compile.");
        
        visit(ast_root); 

        if (llvm::verifyModule(*llvmModule, &llvm::errs()))
        {
            log_error("LLVM Module verification failed. Dumping potentially corrupt IR.");
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
        if (!llvmModule) log_error("Attempted to take a null module. Compile AST first.");
        return std::move(llvmModule);
    }

    [[noreturn]] void ScriptCompiler::log_error(const std::string &message, std::optional<SourceLocation> loc)
    {
        std::stringstream ss;
        ss << "Script Compiler Error";
        if (loc) ss << " " << loc->to_string();
        ss << ": " << message;
        llvm::errs() << ss.str() << "\n";
        throw std::runtime_error(ss.str());
    }

    std::string ScriptCompiler::llvm_type_to_string(llvm::Type *type) const
    {
        if (!type) return "<null llvm type>";
        std::string typeStr;
        llvm::raw_string_ostream rso(typeStr);
        type->print(rso);
        return rso.str();
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

    llvm::GenericValue ScriptCompiler::jit_execute_function(
        const std::string &function_name,
        const std::vector<llvm::GenericValue> &args)
    {
        initialize_jit_engine_dependencies();
        if (!llvmModule) log_error("No LLVM module available for JIT.");
        
        std::string errStr;
        llvm::EngineBuilder engineBuilder(take_module());
        engineBuilder.setErrorStr(&errStr);
        engineBuilder.setEngineKind(llvm::EngineKind::JIT); 
        engineBuilder.setVerifyModules(true); // Enable JIT's own verification
        engineBuilder.setOptLevel(llvm::CodeGenOptLevel::None); // Disable JIT optimizations
        llvm::ExecutionEngine *ee = engineBuilder.create();
        if (!ee) log_error("Failed to construct ExecutionEngine: " + errStr);

        for (const auto &binding : get_runtime_bindings()) {
            if (binding.c_function_pointer == nullptr) continue;
            ee->addGlobalMapping(binding.ir_function_name, reinterpret_cast<uint64_t>(binding.c_function_pointer));
        }
        ee->finalizeObject();
        
        llvm::Function *funcToRun = ee->FindFunctionNamed(function_name);
        if (!funcToRun) { delete ee; log_error("JIT: Function '" + function_name + "' not found."); }

        // Diagnostic check for the function's return type
        llvm::Type* returnType = funcToRun->getReturnType();
        if (!returnType) {
            delete ee;
            log_error("JIT: Function '" + function_name + "' has NULL return type!");
        }
        llvm::errs() << "JIT: Preparing to run function '" << function_name << "'. ";
        if (returnType->isIntegerTy()) {
            llvm::IntegerType* intReturnType = llvm::cast<llvm::IntegerType>(returnType);
            llvm::errs() << "Return type is i" << intReturnType->getBitWidth() << ".\n";
        } else if (returnType->isVoidTy()) {
            llvm::errs() << "Return type is void.\n";
        } else {
            std::string retTypeStr;
            llvm::raw_string_ostream rso(retTypeStr);
            returnType->print(rso);
            rso.flush(); // Ensure the string is fully written
            llvm::errs() << "Return type is " << retTypeStr << " (not integer or void).\n";
        }
        // End diagnostic check

        llvm::GenericValue result; 
        try { result = ee->runFunction(funcToRun, args); }
        catch (const std::exception &e) { delete ee; log_error("JIT: Exception: " + std::string(e.what())); }
        catch (...) { delete ee; log_error("JIT: Unknown exception."); }
        delete ee;
        return result;
    }

    void ScriptCompiler::initialize_aot_engine_dependencies() { 
        if(!aot_initialized) {
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

        auto CPU = llvm::sys::getHostCPUName().str(); // Use specific host CPU
        
        llvm::StringMap<bool> HostFeatures;
        std::string FeaturesStr;
        if (llvm::sys::getHostCPUFeatures(HostFeatures)) { // Populates HostFeatures
            llvm::raw_string_ostream OS(FeaturesStr);
            bool first = true;
            for (auto &Feature : HostFeatures) {
                if (!first) {
                    OS << ",";
                }
                first = false;
                OS << (Feature.getValue() ? "+" : "-") << Feature.getKey();
            }
        }
        // auto Features = FeaturesStr; // This was the problematic line in my thought process, FeaturesStr is already the string.

        llvm::TargetOptions opt;
        // Use a specific relocation model, e.g., Static. PIC_ might also be common.
        // Or, some createTargetMachine overloads might not require it explicitly if a default is fine.
        // Let's try with Reloc::Static as a common default.
        // If your LLVM version's createTargetMachine doesn't take RM or takes it differently, this might need adjustment.
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
        // Corrected enum for object file type
        auto FileType = llvm::CodeGenFileType::ObjectFile; 

        if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
            log_error("TargetMachine can't emit a file of this type.");
            return;
        }

        pass.run(*llvmModule);
        dest.flush();
        // llvm::outs() << "Object file written to " << output_filename << "\n"; // Optional success message
    }


    llvm::Value* ScriptCompiler::visit(std::shared_ptr<AstNode> node)
    {
        if (!node) log_error("Attempted to visit a null AST node.");

        if (auto exprNode = std::dynamic_pointer_cast<ExpressionNode>(node))
            return visit(exprNode).value; 

        if (auto specificNode = std::dynamic_pointer_cast<CompilationUnitNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ClassDeclarationNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<BlockStatementNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<LocalVariableDeclarationStatementNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ExpressionStatementNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<IfStatementNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ReturnStatementNode>(node)) return visit(specificNode);
        
        log_error("Unhandled AST node type in generic visit.", node->location);
        return nullptr;
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<ExpressionNode> node)
    {
        if (!node) log_error("Attempted to visit a null ExpressionNode.");
        if (auto specificNode = std::dynamic_pointer_cast<LiteralExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<IdentifierExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<BinaryExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<AssignmentExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<UnaryExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<MethodCallExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ThisExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<CastExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node)) return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ParenthesizedExpressionNode>(node)) return visit(specificNode); // Added this line
        log_error("Unhandled ExpressionNode type.", node->location);
        return ExpressionVisitResult(nullptr);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<ParenthesizedExpressionNode> node)
    {
        if (!node || !node->expression) {
            log_error("ParenthesizedExpressionNode or its inner expression is null.", node ? node->location : std::nullopt);
            return ExpressionVisitResult(nullptr);
        }
        // Simply visit the inner expression
        return visit(node->expression);
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<StatementNode> node)
    {
        return visit(std::static_pointer_cast<AstNode>(node));
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<CompilationUnitNode> node)
    {
        for (const auto &member : node->externs) {
            if (auto externDecl = std::dynamic_pointer_cast<ExternalMethodDeclarationNode>(member)) visit(externDecl);
            else log_error("Unsupported top-level member (extern).", member->location);
        }
        for (const auto &member : node->members) {
            if (auto nsDecl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member)) visit(nsDecl);
            else if (auto classDecl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) visit(classDecl);
            else log_error("Unsupported top-level member (main).", member->location);
        }
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<NamespaceDeclarationNode> node)
    {
        for (const auto &member : node->members) {
            if (auto classDecl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) visit(classDecl);
            else log_error("Unsupported namespace member.", member->location);
        }
        return nullptr;
    }

    void ScriptCompiler::visit(std::shared_ptr<ExternalMethodDeclarationNode> node) { 
        // Implementation remains largely the same as it relies on get_llvm_type which now handles opaque pointers
        if (!node->type.has_value()) log_error("External method lacks return type.", node->location);
        llvm::Type *return_type = get_llvm_type(node->type.value());
        std::vector<llvm::Type *> param_types;
        for (const auto &param_node : node->parameters) {
            if (!param_node->type) log_error("External method param lacks type.", param_node->location);
            param_types.push_back(get_llvm_type(param_node->type));
        }
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_types, false);
        llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, node->name->name, llvmModule.get());
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ClassDeclarationNode> node)
    {
        std::string class_name = node->name->name;
        if (classTypeRegistry.count(class_name)) { log_error("Class '" + class_name + "' already defined.", node->location); return nullptr; }

        ClassTypeInfo cti; 
        cti.name = class_name;
        cti.type_id = next_type_id++;
        std::vector<llvm::Type*> field_llvm_types_for_struct; // For StructType::create
        unsigned field_idx_counter = 0;

        for (const auto &member : node->members) {
            if (auto fieldDecl = std::dynamic_pointer_cast<FieldDeclarationNode>(member)) {
                if (!fieldDecl->type) { log_error("Field missing type in " + class_name, fieldDecl->location); continue; }
                llvm::Type* actual_field_llvm_type = get_llvm_type(fieldDecl->type.value()); // This will be 'ptr' for class types
                if (!actual_field_llvm_type) { log_error("Could not get LLVM type for field in " + class_name, fieldDecl->type.value()->location); continue; }
                
                for(const auto& declarator : fieldDecl->declarators) {
                    field_llvm_types_for_struct.push_back(actual_field_llvm_type);
                    cti.field_names_in_order.push_back(declarator->name->name); 
                    cti.field_indices[declarator->name->name] = field_idx_counter++; 
                    cti.field_ast_types.push_back(fieldDecl->type.value());
                }
            }
        }
        cti.fieldsType = llvm::StructType::create(*llvmContext, field_llvm_types_for_struct, class_name + "_Fields");
        if (!cti.fieldsType) { log_error("Failed to create fields struct for " + class_name, node->location); return nullptr; }
        classTypeRegistry[class_name] = cti; 

        for (const auto &member : node->members) {
            if (auto methodDecl = std::dynamic_pointer_cast<MethodDeclarationNode>(member)) visit_method_declaration(methodDecl, class_name); 
            else if (auto ctorDecl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member)) visit(ctorDecl, class_name); 
        }
        return nullptr;
    }

    llvm::Function *ScriptCompiler::visit_method_declaration(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name)
    {
        namedValues.clear(); current_function_arc_locals.clear(); 
        bool is_static = false; /* ... check modifiers ... */
        for (auto mod : node->modifiers) if (mod.first == ModifierKind::Static) is_static = true;

        if (!node->type.has_value()) log_error("Method lacks return type.", node->location);
        llvm::Type *return_type = get_llvm_type(node->type.value());
        std::vector<llvm::Type *> param_llvm_types;
        const ClassTypeInfo* this_class_info = nullptr;

        if (!is_static) {
            auto cti_it = classTypeRegistry.find(class_name);
            if (cti_it == classTypeRegistry.end()) { log_error("Class not found for instance method: " + class_name, node->location); return nullptr; }
            this_class_info = &cti_it->second;
            param_llvm_types.push_back(llvm::PointerType::getUnqual(*llvmContext)); // 'this' is opaque ptr
        }
        for (const auto &param_node : node->parameters) {
             if (!param_node->type) log_error("Method param lacks type.", param_node->location);
             param_llvm_types.push_back(get_llvm_type(param_node->type));
        }
        
        std::string func_name = class_name + "." + node->name->name;
        if (func_name == "Program.Main") func_name = "main";
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());
        currentFunction = function;
        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);

        auto llvm_arg_it = function->arg_begin();
        if (!is_static) {
            VariableInfo thisVarInfo;
            thisVarInfo.alloca = create_entry_block_alloca(function, "this", llvm_arg_it->getType()); // getType() is 'ptr'
            thisVarInfo.classInfo = this_class_info;
            llvmBuilder->CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
            namedValues["this"] = thisVarInfo;
            ++llvm_arg_it;
        }
        unsigned ast_param_idx = 0;
        for (; llvm_arg_it != function->arg_end(); ++llvm_arg_it, ++ast_param_idx) {
            VariableInfo paramVarInfo;
            paramVarInfo.alloca = create_entry_block_alloca(function, node->parameters[ast_param_idx]->name->name, llvm_arg_it->getType());
            paramVarInfo.declaredTypeNode = node->parameters[ast_param_idx]->type;
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&paramVarInfo.declaredTypeNode->name_segment)) {
                auto cti_it = classTypeRegistry.find((*identNode)->name);
                if (cti_it != classTypeRegistry.end()) paramVarInfo.classInfo = &cti_it->second;
            }
            llvmBuilder->CreateStore(&*llvm_arg_it, paramVarInfo.alloca);
            namedValues[node->parameters[ast_param_idx]->name->name] = paramVarInfo;
        }

        if (node->body) {
            visit(node->body.value()); // This is a BlockStatementNode
            if (!llvmBuilder->GetInsertBlock()->getTerminator()) {
                for (auto const& [_, header_val] : current_function_arc_locals) if (header_val) llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_release"), {header_val});
                if (return_type->isVoidTy()) llvmBuilder->CreateRetVoid();
                else log_error("Non-void function '" + func_name + "' missing return.", node->body.value()->location);
            }
        } else { 
            log_error("Method '" + func_name + "' has no body.", node->location);
            if (return_type->isVoidTy() && !entry_block->getTerminator()) llvmBuilder->CreateRetVoid();
        }
        return function; 
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<BlockStatementNode> node)
    {
        llvm::Value *last_val = nullptr;
        for (const auto &stmt : node->statements) {
            if (llvmBuilder->GetInsertBlock()->getTerminator()) break;
            last_val = visit(stmt);
        }
        return last_val;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<LocalVariableDeclarationStatementNode> node)
    {
        llvm::Type *var_llvm_type = get_llvm_type(node->type); // This is the LLVM type (e.g. i32, ptr)
        const ClassTypeInfo* var_static_class_info = nullptr; // Static type info if it's a class
        if (var_llvm_type->isPointerTy()) { // If LLVM type is ptr, it might be a class
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment)) {
                auto cti_it = classTypeRegistry.find((*identNode)->name);
                if (cti_it != classTypeRegistry.end()) var_static_class_info = &cti_it->second;
            }
        }

        for (const auto &declarator : node->declarators) {
            VariableInfo varInfo;
            varInfo.alloca = create_entry_block_alloca(currentFunction, declarator->name->name, var_llvm_type);
            varInfo.classInfo = var_static_class_info; // Store static class info
            varInfo.declaredTypeNode = node->type;
            namedValues[declarator->name->name] = varInfo;

            if (declarator->initializer) {
                ExpressionVisitResult init_res = visit(declarator->initializer.value());
                llvm::Value *init_val = init_res.value; // LLVM value of initializer
                const ClassTypeInfo* init_val_class_info = init_res.classInfo; // Static class info of initializer

                if (!init_val) { log_error("Initializer for " + declarator->name->name + " failed.", declarator->initializer.value()->location); continue; }
                
                // LLVM Type Check (e.g. i32 vs ptr)
                if (init_val->getType() != var_llvm_type) {
                    // ... (coercion for primitives, or error if incompatible like i32 to ptr) ...
                     log_error("LLVM type mismatch for initializer of " + declarator->name->name, declarator->initializer.value()->location);
                }
                // Static Type Check (e.g. MyClass vs OtherClass)
                if (var_static_class_info && init_val_class_info && var_static_class_info != init_val_class_info) {
                    // TODO: Check for subtyping if/when supported
                    log_error("Static type mismatch: cannot assign " + init_val_class_info->name + " to " + var_static_class_info->name, declarator->initializer.value()->location);
                }


                llvmBuilder->CreateStore(init_val, varInfo.alloca);
                // ARC: if var is a class type and initializer is also a (compatible) class type
                if (var_static_class_info && var_static_class_info->fieldsType && init_val_class_info && init_val->getType()->isPointerTy()) {
                    llvm::Value* header_ptr_val = getHeaderPtrFromFieldsPtr(init_val, var_static_class_info->fieldsType); // Use var's fieldsType for safety
                    if (header_ptr_val) current_function_arc_locals[varInfo.alloca] = header_ptr_val;
                }
            }
        }
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ExpressionStatementNode> node) { return visit(node->expression).value; }
    
    llvm::Value *ScriptCompiler::visit(std::shared_ptr<IfStatementNode> node) { 
        ExpressionVisitResult cond_res = visit(node->condition);
        // ... (rest of the logic using cond_res.value) ...
        return nullptr; 
    }
    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ReturnStatementNode> node) { 
        if (node->expression) {
            ExpressionVisitResult ret_res = visit(node->expression.value());
            // ... (type checking ret_res.value, ARC cleanup) ...
            llvmBuilder->CreateRet(ret_res.value);
        } else {
            // ... (ARC cleanup) ...
            llvmBuilder->CreateRetVoid();
        }
        return nullptr; 
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<LiteralExpressionNode> node) { 
        llvm::Value* val = nullptr;
        const ClassTypeInfo* ci = nullptr;

        switch (node->kind) {
            case LiteralKind::Integer:
                try { val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvmContext), static_cast<int32_t>(std::stoll(node->valueText)), true); }
                catch (const std::exception& e) { log_error("Invalid int literal: " + node->valueText, node->location); }
                break;
            case LiteralKind::Long:
                try { val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), static_cast<int64_t>(std::stoll(node->valueText)), true); }
                catch (const std::exception& e) { log_error("Invalid long literal: " + node->valueText, node->location); }
                break;
            case LiteralKind::Float:
                try { val = llvm::ConstantFP::get(llvm::Type::getFloatTy(*llvmContext), llvm::APFloat(std::stof(node->valueText))); }
                catch (const std::exception& e) { log_error("Invalid float literal: " + node->valueText, node->location); }
                break;
            case LiteralKind::Double:
                try { val = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*llvmContext), llvm::APFloat(std::stod(node->valueText))); }
                catch (const std::exception& e) { log_error("Invalid double literal: " + node->valueText, node->location); }
                break;
            case LiteralKind::Boolean:
                val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*llvmContext), (node->valueText == "true"));
                break;
            case LiteralKind::Char:
                if (node->valueText.length() == 1) {
                    val = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*llvmContext), node->valueText[0]);
                } else {
                    log_error("Invalid char literal: " + node->valueText, node->location);
                }
                break;
            case LiteralKind::String:
            {
                // 1. Create a global string constant for the literal's content.
                //    The valueText from LiteralExpressionNode is already unescaped.
                llvm::Constant *strConst = llvm::ConstantDataArray::getString(*llvmContext, node->valueText, true); // Add null terminator
                auto *globalVar = new llvm::GlobalVariable(*llvmModule, strConst->getType(), true, llvm::GlobalValue::PrivateLinkage, strConst, ".str");
                globalVar->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
                globalVar->setAlignment(llvm::MaybeAlign(1));

                // 2. Get a pointer to this global string (char*).
                llvm::Value *charPtr = llvmBuilder->CreateBitCast(globalVar, llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext)));
                
                // 3. Get the length of the string.
                llvm::Value *lenVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), node->valueText.length());

                // 4. Call Mycelium_String_new_from_literal.
                llvm::Function *newStrFunc = llvmModule->getFunction("Mycelium_String_new_from_literal");
                if (!newStrFunc) {
                    log_error("Runtime function Mycelium_String_new_from_literal not found.", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                val = llvmBuilder->CreateCall(newStrFunc, {charPtr, lenVal}, "new_mycelium_str");
                // The result 'val' is an opaque pointer (ptr) to MyceliumString.
                // We don't have a ClassTypeInfo for MyceliumString in the classTypeRegistry,
                // but we know its LLVM type is myceliumStringType (struct) and pointers to it are opaque.
                // For now, ci remains nullptr as it's a runtime-managed type not a user-defined class.
            }
                break;
            case LiteralKind::Null:
                // For 'null', the LLVM value is a null pointer of opaque pointer type.
                // The specific type might need to be context-dependent (e.g., if assigning to MyClass* vs OtherClass*).
                // For now, a generic opaque null pointer.
                val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*llvmContext));
                break;
            default:
                log_error("Unhandled literal kind.", node->location);
                break;
        }
        return ExpressionVisitResult(val, ci);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<IdentifierExpressionNode> node)
    {
        auto it = namedValues.find(node->identifier->name);
        if (it == namedValues.end()) { log_error("Undefined variable: " + node->identifier->name, node->location); return ExpressionVisitResult(nullptr); }
        const VariableInfo& varInfo = it->second;
        llvm::Value* loaded_val = llvmBuilder->CreateLoad(varInfo.alloca->getAllocatedType(), varInfo.alloca, node->identifier->name.c_str());
        return ExpressionVisitResult(loaded_val, varInfo.classInfo); // Pass ClassInfo of the variable
    }
    
    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<BinaryExpressionNode> node) {
        ExpressionVisitResult L_res = visit(node->left);
        ExpressionVisitResult R_res = visit(node->right);

        llvm::Value *L = L_res.value;
        llvm::Value *R = R_res.value;

        if (!L || !R) {
            log_error("One or both operands of binary expression are null.", node->location);
            return ExpressionVisitResult(nullptr);
        }

        llvm::Type *LType = L->getType();
        llvm::Type *RType = R->getType();

        // Handle string concatenation separately
        if (node->opKind == BinaryOperatorKind::Add &&
            LType == getMyceliumStringPtrTy() && RType == getMyceliumStringPtrTy()) {
            llvm::Function *concatFunc = llvmModule->getFunction("Mycelium_String_concat");
            if (!concatFunc) {
                log_error("Runtime function Mycelium_String_concat not found.", node->location);
                return ExpressionVisitResult(nullptr);
            }
            llvm::Value* result_str_ptr = llvmBuilder->CreateCall(concatFunc, {L, R}, "concat_str");
            return ExpressionVisitResult(result_str_ptr, nullptr); // Result is a MyceliumString*, no specific ClassTypeInfo here
        }
        
        // TODO: Handle string + primitive or primitive + string concatenation by calling conversion functions.
        // Example: if L is string and R is int, call Mycelium_String_from_int(R) then concat.
        if (node->opKind == BinaryOperatorKind::Add) {
            if (LType == getMyceliumStringPtrTy() && RType->isIntegerTy(32)) { // string + int
                llvm::Function* fromIntFunc = llvmModule->getFunction("Mycelium_String_from_int");
                if (!fromIntFunc) { log_error("Mycelium_String_from_int not found", node->right->location); return ExpressionVisitResult(nullptr); }
                llvm::Value* r_as_str = llvmBuilder->CreateCall(fromIntFunc, {R}, "int_to_str_tmp");
                
                llvm::Function *concatFunc = llvmModule->getFunction("Mycelium_String_concat");
                if (!concatFunc) { log_error("Mycelium_String_concat not found", node->location); return ExpressionVisitResult(nullptr); }
                llvm::Value* result_str_ptr = llvmBuilder->CreateCall(concatFunc, {L, r_as_str}, "concat_str_int");
                // TODO: ARC release for r_as_str if it's heap allocated by fromIntFunc
                return ExpressionVisitResult(result_str_ptr, nullptr);
            }
            // Add other combinations: int + string, string + double, etc.
        }


        // For other operations, ensure types are compatible or can be coerced.
        // For simplicity, this example assumes L and R have compatible types for the operation.
        // A real compiler would have more sophisticated type checking and coercion.
        if (LType != RType) {
            // Basic coercion: if one is float/double and other is int, promote int to float/double
            // This is a very simplified coercion logic.
            if (LType->isFloatingPointTy() && RType->isIntegerTy()) {
                R = llvmBuilder->CreateSIToFP(R, LType, "inttofp_tmp");
                RType = LType;
            } else if (RType->isFloatingPointTy() && LType->isIntegerTy()) {
                L = llvmBuilder->CreateSIToFP(L, RType, "inttofp_tmp");
                LType = RType;
            } else {
                 log_error("Type mismatch in binary expression: " + llvm_type_to_string(LType) + " vs " + llvm_type_to_string(RType), node->location);
                 return ExpressionVisitResult(nullptr);
            }
        }


        llvm::Value *result_val = nullptr;
        switch (node->opKind) {
            case BinaryOperatorKind::Add:
                if (LType->isIntegerTy()) result_val = llvmBuilder->CreateAdd(L, R, "addtmp");
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFAdd(L, R, "faddtmp");
                else log_error("Unsupported type for Add: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::Subtract:
                if (LType->isIntegerTy()) result_val = llvmBuilder->CreateSub(L, R, "subtmp");
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFSub(L, R, "fsubtmp");
                else log_error("Unsupported type for Subtract: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::Multiply:
                if (LType->isIntegerTy()) result_val = llvmBuilder->CreateMul(L, R, "multmp");
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFMul(L, R, "fmultmp");
                else log_error("Unsupported type for Multiply: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::Divide:
                if (LType->isIntegerTy()) result_val = llvmBuilder->CreateSDiv(L, R, "sdivtmp"); // Signed division
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFDiv(L, R, "fdivtmp");
                else log_error("Unsupported type for Divide: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::Modulo:
                 if (LType->isIntegerTy()) result_val = llvmBuilder->CreateSRem(L, R, "sremtmp"); // Signed remainder
                else log_error("Unsupported type for Modulo: " + llvm_type_to_string(LType), node->location);
                break;
            // Comparisons
            case BinaryOperatorKind::Equals:
                if (LType->isIntegerTy() || LType->isPointerTy()) result_val = llvmBuilder->CreateICmpEQ(L, R, "eqtmp");
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFCmpOEQ(L, R, "feqtmp"); // Ordered, Equal
                else log_error("Unsupported type for Equals: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::NotEquals:
                if (LType->isIntegerTy() || LType->isPointerTy()) result_val = llvmBuilder->CreateICmpNE(L, R, "netmp");
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFCmpONE(L, R, "fnetmp"); // Ordered, Not Equal
                else log_error("Unsupported type for NotEquals: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::LessThan:
                if (LType->isIntegerTy()) result_val = llvmBuilder->CreateICmpSLT(L, R, "slttmp"); // Signed Less Than
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFCmpOLT(L, R, "folttmp"); // Ordered, Less Than
                else log_error("Unsupported type for LessThan: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::GreaterThan:
                if (LType->isIntegerTy()) result_val = llvmBuilder->CreateICmpSGT(L, R, "sgttmp"); // Signed Greater Than
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFCmpOGT(L, R, "fogttmp"); // Ordered, Greater Than
                else log_error("Unsupported type for GreaterThan: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::LessThanOrEqual:
                if (LType->isIntegerTy()) result_val = llvmBuilder->CreateICmpSLE(L, R, "sletmp"); // Signed Less or Equal
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFCmpOLE(L, R, "foletmp"); // Ordered, Less or Equal
                else log_error("Unsupported type for LessThanOrEqual: " + llvm_type_to_string(LType), node->location);
                break;
            case BinaryOperatorKind::GreaterThanOrEqual:
                if (LType->isIntegerTy()) result_val = llvmBuilder->CreateICmpSGE(L, R, "sgetmp"); // Signed Greater or Equal
                else if (LType->isFloatingPointTy()) result_val = llvmBuilder->CreateFCmpOGE(L, R, "fogetmp"); // Ordered, Greater or Equal
                else log_error("Unsupported type for GreaterThanOrEqual: " + llvm_type_to_string(LType), node->location);
                break;
            // Logical operators (assuming short-circuiting is handled by IfStatement or similar for &&, ||)
            // If these are bitwise AND/OR, then use CreateAnd, CreateOr.
            // For logical && and || on bools (i1), they are often lowered to branches.
            // If they are simple non-short-circuiting logical ops on i1:
            case BinaryOperatorKind::LogicalAnd: // Assuming inputs are i1 (bool)
                if (LType->isIntegerTy(1) && RType->isIntegerTy(1)) result_val = llvmBuilder->CreateAnd(L, R, "andtmp");
                else log_error("LogicalAnd requires boolean operands.", node->location);
                break;
            case BinaryOperatorKind::LogicalOr: // Assuming inputs are i1 (bool)
                 if (LType->isIntegerTy(1) && RType->isIntegerTy(1)) result_val = llvmBuilder->CreateOr(L, R, "ortmp");
                else log_error("LogicalOr requires boolean operands.", node->location);
                break;
            default:
                log_error("Unsupported binary operator.", node->location);
                return ExpressionVisitResult(nullptr);
        }
        // The result of a comparison is always i1 (bool). Other ops retain operand type (after coercion).
        // ClassInfo for the result is typically null unless the operation results in a new object instance
        // (e.g., overloaded operator returning a new class type, not handled here).
        return ExpressionVisitResult(result_val, nullptr);
    }
    
    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<AssignmentExpressionNode> node)
    {
        ExpressionVisitResult source_res = visit(node->source);
        llvm::Value *new_llvm_val = source_res.value;
        const ClassTypeInfo* new_val_static_ci = source_res.classInfo;
        if (!new_llvm_val) { log_error("Assignment source is null.", node->source->location); return ExpressionVisitResult(nullptr); }

        if (auto id_target = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target)) {
            auto it = namedValues.find(id_target->identifier->name);
            if (it == namedValues.end()) { log_error("Assigning to undeclared var: " + id_target->identifier->name, id_target->location); return ExpressionVisitResult(nullptr); }
            VariableInfo& target_var_info = it->second; // Modifying VariableInfo if needed (though not here)
            llvm::Type* target_llvm_type = target_var_info.alloca->getAllocatedType();
            const ClassTypeInfo* target_static_ci = target_var_info.classInfo;

            // LLVM Type Check & Static Type Check
            if (new_llvm_val->getType() != target_llvm_type) { /* ... error or coerce for primitives ... */ }
            if (target_static_ci && new_val_static_ci && target_static_ci != new_val_static_ci) { /* ... error for incompatible classes ... */ }
            
            // ARC
            if (new_val_static_ci && new_val_static_ci->fieldsType) { /* Retain new_llvm_val */ 
                llvm::Value* new_hdr = getHeaderPtrFromFieldsPtr(new_llvm_val, new_val_static_ci->fieldsType);
                if(new_hdr) llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_retain"), {new_hdr});
            }
            if (target_static_ci && target_static_ci->fieldsType) { /* Release old value in target_var_info.alloca */ 
                llvm::Value* old_val = llvmBuilder->CreateLoad(target_llvm_type, target_var_info.alloca);
                llvm::Value* old_hdr = getHeaderPtrFromFieldsPtr(old_val, target_static_ci->fieldsType);
                if(old_hdr) llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_release"), {old_hdr});
            }
            llvmBuilder->CreateStore(new_llvm_val, target_var_info.alloca);
            // Update ARC tracking for the local variable's alloca
            if (new_val_static_ci && new_val_static_ci->fieldsType) {
                 current_function_arc_locals[target_var_info.alloca] = getHeaderPtrFromFieldsPtr(new_llvm_val, new_val_static_ci->fieldsType);
            } else {
                 current_function_arc_locals.erase(target_var_info.alloca);
            }

        } else if (auto member_target = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target)) {
            ExpressionVisitResult obj_res = visit(member_target->target); // This is the object (e.g. myObj in myObj.field)
            if (!obj_res.value || !obj_res.classInfo || !obj_res.classInfo->fieldsType) { log_error("Invalid member assignment target.", member_target->target->location); return ExpressionVisitResult(nullptr); }
            
            auto field_it = obj_res.classInfo->field_indices.find(member_target->memberName->name);
            if (field_it == obj_res.classInfo->field_indices.end()) { /* error */ return ExpressionVisitResult(nullptr); }
            unsigned field_idx = field_it->second;
            llvm::Type* field_llvm_type = obj_res.classInfo->fieldsType->getElementType(field_idx);
            const ClassTypeInfo* field_static_ci = nullptr; // Get from obj_res.classInfo->field_ast_types[field_idx]
            // ... (Type checks, ARC for field) ...
            llvm::Value* field_ptr = llvmBuilder->CreateStructGEP(obj_res.classInfo->fieldsType, obj_res.value, field_idx);
            llvmBuilder->CreateStore(new_llvm_val, field_ptr);
        } else { log_error("Invalid assignment target.", node->target->location); return ExpressionVisitResult(nullptr); }
        return ExpressionVisitResult(new_llvm_val, new_val_static_ci);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<UnaryExpressionNode> node) { 
        ExpressionVisitResult operand_res = visit(node->operand);
        // ... (logic using operand_res.value) ...
        return ExpressionVisitResult(nullptr); // Placeholder
    }
    
    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<MethodCallExpressionNode> node)
    {
        std::string resolved_func_name;
        llvm::Value* instance_ptr_for_call = nullptr; 
        const ClassTypeInfo* callee_class_info = nullptr; // For determining return type's ClassInfo if method returns object

        if (auto memberAccess = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target)) {
            // Target is like obj.method or Class.method
            std::shared_ptr<ExpressionNode> lhs_of_dot = memberAccess->target;
            std::string member_name_str = memberAccess->memberName->name;

            if (auto class_ident_node = std::dynamic_pointer_cast<IdentifierExpressionNode>(lhs_of_dot)) {
                // LHS is a simple identifier. Could be a class name (static call) or an instance variable.
                std::string lhs_name = class_ident_node->identifier->name;
                auto cti_it = classTypeRegistry.find(lhs_name);
                if (cti_it != classTypeRegistry.end()) {
                    // It's a class name: Static call like Point.CreateOrigin()
                    callee_class_info = &cti_it->second;
                    resolved_func_name = callee_class_info->name + "." + member_name_str;
                    instance_ptr_for_call = nullptr; // No 'this' for static calls
                } else {
                    // Not a class name, so must be an instance variable: p1.Print()
                    ExpressionVisitResult target_obj_res = visit(lhs_of_dot); // Evaluate the instance variable
                    if (target_obj_res.value && target_obj_res.classInfo) {
                        instance_ptr_for_call = target_obj_res.value;
                        callee_class_info = target_obj_res.classInfo;
                        resolved_func_name = callee_class_info->name + "." + member_name_str;
                    } else {
                        log_error("Cannot call method '" + member_name_str + "' on undefined variable or non-class type '" + lhs_name + "'.", lhs_of_dot->location);
                        return ExpressionVisitResult(nullptr);
                    }
                }
            } else {
                // LHS is a complex expression: (new Point(0,0)).Print()
                ExpressionVisitResult target_obj_res = visit(lhs_of_dot); // Evaluate the complex expression
                if (target_obj_res.value && target_obj_res.classInfo) {
                    instance_ptr_for_call = target_obj_res.value;
                    callee_class_info = target_obj_res.classInfo;
                    resolved_func_name = callee_class_info->name + "." + member_name_str;
                } else {
                    log_error("Cannot call method '" + member_name_str + "' on expression that does not resolve to a class instance.", lhs_of_dot->location);
                    return ExpressionVisitResult(nullptr);
                }
            }
        } else if (auto idTarget = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target)) {
            // Global/external function call like Mycelium_String_print(...)
            resolved_func_name = idTarget->identifier->name;
            // instance_ptr_for_call remains nullptr
            // callee_class_info remains nullptr (not a class method)
        } else {
            log_error("Unsupported method call target type.", node->target->location);
            return ExpressionVisitResult(nullptr);
        }

        if (resolved_func_name.empty()) {
             log_error("Could not resolve function name for method call.", node->target->location);
             return ExpressionVisitResult(nullptr);
        }
        if (resolved_func_name == "Program.Main") resolved_func_name = "main"; // Special case for entry point

        llvm::Function *callee = llvmModule->getFunction(resolved_func_name);
        if (!callee) { log_error("Function not found: " + resolved_func_name, node->target->location); return ExpressionVisitResult(nullptr); }
        
        std::vector<llvm::Value *> args_values;
        if (instance_ptr_for_call) args_values.push_back(instance_ptr_for_call);
        
        if (node->argumentList) {
            for (const auto &arg_node : node->argumentList->arguments) {
                ExpressionVisitResult arg_res = visit(arg_node->expression);
                if (!arg_res.value) { log_error("Method call argument failed to compile.", arg_node->location); return ExpressionVisitResult(nullptr); }
                args_values.push_back(arg_res.value);
            }
        }
        // TODO: Arity and type checking of args against callee->getFunctionType()
        llvm::Value* call_result_val = llvmBuilder->CreateCall(callee, args_values, callee->getReturnType()->isVoidTy() ? "" : "calltmp");
        
        const ClassTypeInfo* return_static_ci = nullptr;
        // TODO: Determine ClassTypeInfo for return_val if it's an object, using callee's AST return type.
        return ExpressionVisitResult(call_result_val, return_static_ci);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<ObjectCreationExpressionNode> node)
    {
        if (!node->type) { log_error("Object creation missing type.", node->location); return ExpressionVisitResult(nullptr); }
        std::string class_name_str; 
        if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment)) class_name_str = (*identNode)->name;
        else { log_error("Unsupported type in new.", node->type->location); return ExpressionVisitResult(nullptr); }

        auto cti_it = classTypeRegistry.find(class_name_str);
        if (cti_it == classTypeRegistry.end()) { log_error("Undefined class in new: " + class_name_str, node->type->location); return ExpressionVisitResult(nullptr); }
        const ClassTypeInfo& cti = cti_it->second;
        if (!cti.fieldsType) { log_error("Class " + class_name_str + " has no fieldsType.", node->type->location); return ExpressionVisitResult(nullptr); }
        
        llvm::DataLayout dl = llvmModule->getDataLayout();
        llvm::Value* data_size_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), dl.getTypeAllocSize(cti.fieldsType));
        llvm::Value* type_id_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvmContext), cti.type_id);
        llvm::Function* alloc_func = llvmModule->getFunction("Mycelium_Object_alloc");
        if (!alloc_func) { log_error("Runtime Mycelium_Object_alloc not found.", node->location); return ExpressionVisitResult(nullptr); }
        llvm::Value* header_ptr_val = llvmBuilder->CreateCall(alloc_func, {data_size_val, type_id_val}, "new.header"); // This is ptr to MyceliumObjectHeader
        
        // Get the opaque ptr to the fields struct (which is what user code manipulates)
        llvm::Value* fields_obj_opaque_ptr = getFieldsPtrFromHeaderPtr(header_ptr_val, cti.fieldsType);

        std::string ctor_name_str = class_name_str + ".%ctor"; // Add mangling for overloads based on arg types
        std::vector<llvm::Value *> ctor_args_values = {fields_obj_opaque_ptr}; // 'this' is the first argument (the fields ptr)
        if (node->argumentList.has_value()) {
            for(const auto& arg_node : node->argumentList.value()->arguments) {
                ctor_args_values.push_back(visit(arg_node->expression).value);
            }
            // TODO: Mangle ctor_name_str based on argument types/count
        }
        
        llvm::Function* constructor_func = llvmModule->getFunction(ctor_name_str);
        if (!constructor_func) { log_error("Constructor " + ctor_name_str + " not found.", node->location); return ExpressionVisitResult(nullptr); }
        llvmBuilder->CreateCall(constructor_func, ctor_args_values);
        return ExpressionVisitResult(fields_obj_opaque_ptr, &cti); // Return the opaque ptr to fields and its static ClassInfo
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<ThisExpressionNode> node)
    {
        auto it = namedValues.find("this");
        if (it == namedValues.end()) { log_error("'this' used inappropriately.", node->location); return ExpressionVisitResult(nullptr); }
        const VariableInfo& thisVarInfo = it->second; // thisVarInfo.alloca is AllocaInst* for 'ptr'
        llvm::Value* loaded_this_ptr = llvmBuilder->CreateLoad(thisVarInfo.alloca->getAllocatedType(), thisVarInfo.alloca, "this.val");
        return ExpressionVisitResult(loaded_this_ptr, thisVarInfo.classInfo); // classInfo is for 'this'
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<CastExpressionNode> node) {
        ExpressionVisitResult expr_to_cast_res = visit(node->expression);
        llvm::Value* expr_val = expr_to_cast_res.value;

        if (!expr_val) {
            log_error("Expression to be cast is null.", node->expression->location);
            return ExpressionVisitResult(nullptr);
        }

        llvm::Type* target_llvm_type = get_llvm_type(node->targetType);
        if (!target_llvm_type) {
             log_error("Target type for cast is null.", node->targetType->location);
            return ExpressionVisitResult(nullptr);
        }

        const ClassTypeInfo* target_static_ci = nullptr;
        if (target_llvm_type->isPointerTy()) { // Check if the LLVM type is a pointer
            if (auto typeNameNode = node->targetType) { // Ensure targetType is valid
                if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&typeNameNode->name_segment)) {
                    auto cti_it = classTypeRegistry.find((*identNode)->name);
                    if (cti_it != classTypeRegistry.end()) {
                        target_static_ci = &cti_it->second;
                    }
                } else if (auto qnNode = std::get_if<std::shared_ptr<QualifiedNameNode>>(&typeNameNode->name_segment)) {
                    // If it's a qualified name, we'd need to resolve it to a class name string
                    // For simplicity, assuming simple identifier for class names in casts for now
                    // Or, you'd traverse the QualifiedNameNode to get the final identifier.
                    // This part might need more robust qualified name resolution if complex class names are used in casts.
                }
            }
        }


        llvm::Value* cast_val = nullptr;
        llvm::Type* src_llvm_type = expr_val->getType();

        if (target_llvm_type == src_llvm_type) { // No cast needed if types are identical
            cast_val = expr_val;
        }
        // Floating point to Integer
        else if (target_llvm_type->isIntegerTy() && src_llvm_type->isFloatingPointTy()) {
            cast_val = llvmBuilder->CreateFPToSI(expr_val, target_llvm_type, "fptosi_cast");
        }
        // Integer to Floating point
        else if (target_llvm_type->isFloatingPointTy() && src_llvm_type->isIntegerTy()) {
            cast_val = llvmBuilder->CreateSIToFP(expr_val, target_llvm_type, "sitofp_cast");
        }
        // Integer to Integer (different width or signedness)
        else if (target_llvm_type->isIntegerTy() && src_llvm_type->isIntegerTy()) {
            unsigned target_width = target_llvm_type->getIntegerBitWidth();
            unsigned src_width = src_llvm_type->getIntegerBitWidth();
            if (target_width > src_width) {
                // Assuming signed extension for widening. Use CreateZExt for unsigned.
                cast_val = llvmBuilder->CreateSExt(expr_val, target_llvm_type, "sext_cast");
            } else if (target_width < src_width) {
                cast_val = llvmBuilder->CreateTrunc(expr_val, target_llvm_type, "trunc_cast");
            } else { // Same width, but potentially different type (e.g. i8 to char if they are distinct types)
                cast_val = expr_val; // Or BitCast if they are truly different types of same width
            }
        }
        // Pointer to Pointer (e.g., upcast, downcast, or to/from opaque ptr)
        else if (target_llvm_type->isPointerTy() && src_llvm_type->isPointerTy()) {
            cast_val = llvmBuilder->CreateBitCast(expr_val, target_llvm_type, "ptr_bitcast");
        }
        // Pointer to Integer
        else if (target_llvm_type->isIntegerTy() && src_llvm_type->isPointerTy()) {
            cast_val = llvmBuilder->CreatePtrToInt(expr_val, target_llvm_type, "ptrtoint_cast");
        }
        // Integer to Pointer
        else if (target_llvm_type->isPointerTy() && src_llvm_type->isIntegerTy()) {
            cast_val = llvmBuilder->CreateIntToPtr(expr_val, target_llvm_type, "inttoptr_cast");
        }
        // Add other specific casts as needed (e.g., vector types)
        else {
            log_error("Unsupported cast from " + llvm_type_to_string(src_llvm_type) +
                      " to " + llvm_type_to_string(target_llvm_type), node->location);
            return ExpressionVisitResult(nullptr, nullptr);
        }
        return ExpressionVisitResult(cast_val, target_static_ci);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<MemberAccessExpressionNode> node) {
        ExpressionVisitResult target_obj_res = visit(node->target);
        if (!target_obj_res.value || !target_obj_res.classInfo || !target_obj_res.classInfo->fieldsType) {
            log_error("Invalid target for member access.", node->target->location);
            return ExpressionVisitResult(nullptr);
        }
        auto field_it = target_obj_res.classInfo->field_indices.find(node->memberName->name);
        if (field_it == target_obj_res.classInfo->field_indices.end()) {
            log_error("Field " + node->memberName->name + " not found in " + target_obj_res.classInfo->name, node->memberName->location);
            return ExpressionVisitResult(nullptr);
        }
        unsigned field_idx = field_it->second;
        llvm::Type* field_llvm_type = target_obj_res.classInfo->fieldsType->getElementType(field_idx);
        llvm::Value* field_ptr = llvmBuilder->CreateStructGEP(target_obj_res.classInfo->fieldsType, target_obj_res.value, field_idx, node->memberName->name + ".ptr");
        llvm::Value* loaded_field = llvmBuilder->CreateLoad(field_llvm_type, field_ptr, node->memberName->name);
        
        const ClassTypeInfo* field_static_ci = nullptr;
        if (field_llvm_type->isPointerTy() && field_idx < target_obj_res.classInfo->field_ast_types.size()) {
            std::shared_ptr<TypeNameNode> field_ast_type = target_obj_res.classInfo->field_ast_types[field_idx];
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&field_ast_type->name_segment)) {
                auto cti_it = classTypeRegistry.find((*identNode)->name);
                if (cti_it != classTypeRegistry.end()) field_static_ci = &cti_it->second;
            }
        }
        return ExpressionVisitResult(loaded_field, field_static_ci);
    }

    llvm::Function* ScriptCompiler::visit(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name) {
        namedValues.clear();
        current_function_arc_locals.clear();

        // Constructor name mangling (TODO: overload resolution based on params)
        std::string func_name = class_name + ".%ctor"; 

        // Constructor return type is void in LLVM
        llvm::Type *return_type = llvm::Type::getVoidTy(*llvmContext);

        std::vector<llvm::Type *> param_llvm_types;
        const ClassTypeInfo* this_class_info = nullptr;

        auto cti_it = classTypeRegistry.find(class_name);
        if (cti_it == classTypeRegistry.end()) {
            log_error("Class not found for constructor: " + class_name, node->location);
            return nullptr;
        }
        this_class_info = &cti_it->second;

        // First parameter is always 'this' (opaque pointer to the fields struct)
        param_llvm_types.push_back(llvm::PointerType::getUnqual(*llvmContext));

        // Add AST parameters
        for (const auto &param_node : node->parameters) {
            if (!param_node->type) {
                log_error("Constructor parameter lacks type in " + class_name, param_node->location);
                return nullptr; // Or handle error more gracefully
            }
            param_llvm_types.push_back(get_llvm_type(param_node->type));
        }

        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());
        currentFunction = function;

        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);

        auto llvm_arg_it = function->arg_begin();

        // Process 'this' argument (first LLVM argument)
        VariableInfo thisVarInfo;
        thisVarInfo.alloca = create_entry_block_alloca(function, "this.ctor.arg", llvm_arg_it->getType());
        thisVarInfo.classInfo = this_class_info; // ClassInfo for 'this'
        // The 'this' pointer passed to the constructor is already the fields pointer.
        llvmBuilder->CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
        namedValues["this"] = thisVarInfo;
        ++llvm_arg_it;

        // Process regular AST parameters
        unsigned ast_param_idx = 0;
        for (; llvm_arg_it != function->arg_end(); ++llvm_arg_it, ++ast_param_idx) {
            if (ast_param_idx >= node->parameters.size()) {
                 log_error("LLVM argument count mismatch with AST parameters for constructor " + func_name, node->location);
                 // This indicates an internal error or malformed AST/call
                 return function; // Or handle error more robustly
            }
            const auto& ast_param = node->parameters[ast_param_idx];
            VariableInfo paramVarInfo;
            paramVarInfo.alloca = create_entry_block_alloca(function, ast_param->name->name, llvm_arg_it->getType());
            paramVarInfo.declaredTypeNode = ast_param->type;
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&paramVarInfo.declaredTypeNode->name_segment)) {
                auto cti_param_it = classTypeRegistry.find((*identNode)->name);
                if (cti_param_it != classTypeRegistry.end()) {
                    paramVarInfo.classInfo = &cti_param_it->second;
                }
            }
            llvmBuilder->CreateStore(&*llvm_arg_it, paramVarInfo.alloca);
            namedValues[ast_param->name->name] = paramVarInfo;
        }

        if (node->body) {
            visit(node->body.value()); // This is a BlockStatementNode
            if (!llvmBuilder->GetInsertBlock()->getTerminator()) {
                // ARC cleanup for constructor locals before implicit return
                for (auto const& [_, header_val] : current_function_arc_locals) {
                    if (header_val) { // Ensure header_val is not null
                        llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_release"), {header_val});
                    }
                }
                llvmBuilder->CreateRetVoid(); // Constructors implicitly return void
            }
        } else {
            log_error("Constructor '" + func_name + "' has no body.", node->location);
            if (!entry_block->getTerminator()) { // Ensure block is terminated
                 // ARC cleanup for constructor locals before implicit return
                for (auto const& [_, header_val] : current_function_arc_locals) {
                    if (header_val) {
                        llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_release"), {header_val});
                    }
                }
                llvmBuilder->CreateRetVoid();
            }
        }
        return function;
    }

    llvm::Value* ScriptCompiler::getFieldsPtrFromHeaderPtr(llvm::Value* headerPtr, llvm::StructType* fieldsLLVMStructType) {
        if (!myceliumObjectHeaderType) { log_error("MyceliumObjectHeader type not initialized."); return nullptr; }
        llvm::Type* i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext)); // opaque ptr to i8
        llvm::Value* header_as_i8_ptr = llvmBuilder->CreateBitCast(headerPtr, i8PtrTy, "header.i8ptr.forfields");
        uint64_t header_size = llvmModule->getDataLayout().getTypeAllocSize(myceliumObjectHeaderType);
        llvm::Value* offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), header_size);
        llvm::Value* fields_raw_ptr = llvmBuilder->CreateGEP(llvm::Type::getInt8Ty(*llvmContext), header_as_i8_ptr, offset, "fields.rawptr.fromhdr");
        // The fields_raw_ptr is i8*, but it points to the start of the fields.
        // For user code, this should be treated as an opaque 'ptr' that statically refers to the class instance.
        return llvmBuilder->CreateBitCast(fields_raw_ptr, llvm::PointerType::getUnqual(*llvmContext), "fields.ptr.fromhdr"); 
    }

    llvm::Value* ScriptCompiler::getHeaderPtrFromFieldsPtr(llvm::Value* fieldsPtr, llvm::StructType* fieldsLLVMStructType) {
        // fieldsPtr is an opaque 'ptr' that we know points to an instance of fieldsLLVMStructType
        if (!myceliumObjectHeaderType) { log_error("MyceliumObjectHeader type not initialized."); return nullptr; }
        llvm::Type* i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext));
        llvm::Value* fields_as_i8_ptr = llvmBuilder->CreateBitCast(fieldsPtr, i8PtrTy, "fields.i8ptr.forhdr");
        uint64_t header_size = llvmModule->getDataLayout().getTypeAllocSize(myceliumObjectHeaderType);
        llvm::Value* offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), -static_cast<int64_t>(header_size));
        llvm::Value* header_raw_ptr = llvmBuilder->CreateGEP(llvm::Type::getInt8Ty(*llvmContext), fields_as_i8_ptr, offset, "header.rawptr.fromfields");
        // header_raw_ptr is i8*, cast it to an opaque ptr (which is how MyceliumObjectHeader* is represented)
        return llvmBuilder->CreateBitCast(header_raw_ptr, llvm::PointerType::getUnqual(*llvmContext), "header.ptr.fromfields"); 
    }

    llvm::Type *ScriptCompiler::get_llvm_type(std::shared_ptr<TypeNameNode> type_node) {
        if (!type_node) { log_error("TypeNameNode is null."); return nullptr; }
        try { return get_llvm_type_from_string(std::get<std::shared_ptr<IdentifierNode>>(type_node->name_segment)->name, type_node->location); }
        catch (const std::exception &e) { log_error("Error getting type name: " + std::string(e.what()), type_node->location); return nullptr; }
    }

    llvm::Type *ScriptCompiler::get_llvm_type_from_string(const std::string &type_name, std::optional<SourceLocation> loc) {
        if (type_name == "int" || type_name == "i32") return llvm::Type::getInt32Ty(*llvmContext);
        if (type_name == "bool") return llvm::Type::getInt1Ty(*llvmContext);
        if (type_name == "void") return llvm::Type::getVoidTy(*llvmContext);
        if (type_name == "float") return llvm::Type::getFloatTy(*llvmContext);
        if (type_name == "double") return llvm::Type::getDoubleTy(*llvmContext);
        if (type_name == "char" || type_name == "i8") return llvm::Type::getInt8Ty(*llvmContext);
        if (type_name == "long" || type_name == "i64") return llvm::Type::getInt64Ty(*llvmContext);
        
        // For 'string' or any registered class name, the LLVM type is an opaque pointer 'ptr'.
        // The actual struct type (MyceliumString or ClassTypeInfo::fieldsType) is used for GEPs, size calculations, etc.
        if (type_name == "string" || classTypeRegistry.count(type_name)) {
            return llvm::PointerType::getUnqual(*llvmContext); 
        }
        
        log_error("Unknown type name: '" + type_name + "'", loc);
        return nullptr;
    }

    llvm::Type *ScriptCompiler::getMyceliumStringPtrTy() {
        // A pointer to MyceliumString (which is a struct) is an opaque pointer.
        return llvm::PointerType::getUnqual(*llvmContext); 
    }

    llvm::Type *ScriptCompiler::getMyceliumObjectHeaderPtrTy() {
        // A pointer to MyceliumObjectHeader (a struct) is an opaque pointer.
        return llvm::PointerType::getUnqual(*llvmContext); 
    }

    void ScriptCompiler::declare_all_runtime_functions() { 
        if (!llvmModule) { log_error("LLVM module not available for declaring runtime functions."); return; }
        llvm::Type* opaque_ptr_type = llvm::PointerType::getUnqual(*llvmContext);
        for (const auto &binding : get_runtime_bindings()) {
            llvm::FunctionType *func_type = binding.get_llvm_type(*llvmContext, opaque_ptr_type, opaque_ptr_type);
            if (!func_type) { log_error("Failed to get LLVM FunctionType for: " + binding.ir_function_name); continue; }
            llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, binding.ir_function_name, llvmModule.get());
        }
    }
    llvm::AllocaInst *ScriptCompiler::create_entry_block_alloca(llvm::Function *function, const std::string &var_name, llvm::Type *type) {
        llvm::IRBuilder<> TmpB(&function->getEntryBlock(), function->getEntryBlock().getFirstInsertionPt());
        return TmpB.CreateAlloca(type, nullptr, var_name.c_str());
    }

} // namespace Mycelium::Scripting::Lang
