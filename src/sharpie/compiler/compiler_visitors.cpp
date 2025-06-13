#include "sharpie/compiler/script_compiler.hpp"
#include "sharpie/script_ast.hpp" // Includes all ast node headers
#include "sharpie/common/logger.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include <stdexcept>
#include <algorithm>
#include <iostream>

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{
    // --- Visitor Methods (snake_case) ---
    llvm::Value *ScriptCompiler::visit(std::shared_ptr<AstNode> node)
    {
        if (!node)
            log_error("Attempted to visit a null AST node.");
        if (auto specificNode = std::dynamic_pointer_cast<CompilationUnitNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<NamespaceDeclarationNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ClassDeclarationNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ExternalMethodDeclarationNode>(node))
        {
            visit(specificNode);
            return nullptr;
        }
        if (auto specificNode = std::dynamic_pointer_cast<MethodDeclarationNode>(node))
        {
            log_error("Generic visit called for MethodDeclarationNode without class context.", node->location);
            return nullptr;
        }
        if (auto specificNode = std::dynamic_pointer_cast<ConstructorDeclarationNode>(node))
        {
            log_error("Generic visit called for ConstructorDeclarationNode without class context.", node->location);
            return nullptr;
        }
        if (auto specificNode = std::dynamic_pointer_cast<DestructorDeclarationNode>(node))
        {
            log_error("Generic visit called for DestructorDeclarationNode without class context.", node->location);
            return nullptr;
        }
        if (auto specificNode = std::dynamic_pointer_cast<BlockStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<LocalVariableDeclarationStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ExpressionStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<IfStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<WhileStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ForStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ReturnStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<BreakStatementNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ContinueStatementNode>(node))
            return visit(specificNode);
        if (auto exprNode = std::dynamic_pointer_cast<ExpressionNode>(node))
        {
            return visit(exprNode).value;
        }
        log_error("Unhandled AST node type in generic AstNode visit: " + std::string(typeid(*node).name()), node->location);
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<CompilationUnitNode> node)
    {
        // Process external declarations first
        for (const auto &ext_decl : node->externs)
        {
            visit(ext_decl);
        }

        // A helper lambda to recursively traverse namespaces and collect class declarations.
        // It maintains the current namespace path.
        std::function<void(const std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>>&, const std::string&)> collect_classes_recursive;
        
        // We need to store classes with their namespace context to process them correctly.
        std::vector<std::pair<std::shared_ptr<ClassDeclarationNode>, std::string>> all_classes_with_context;

        collect_classes_recursive = 
            [&](const std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>>& members, const std::string& current_namespace)
        {
            for (const auto& member : members)
            {
                if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member))
                {
                    all_classes_with_context.push_back({class_decl, current_namespace});
                }
                else if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member))
                {
                    std::string next_namespace = current_namespace.empty() 
                                                    ? ns_decl->name->name 
                                                    : current_namespace + "." + ns_decl->name->name;
                    collect_classes_recursive(ns_decl->members, next_namespace);
                }
            }
        };

        // Start the collection from the top level (global namespace)
        collect_classes_recursive(node->members, "");

        // PASS 1: Create class structures and declare ALL method signatures across ALL classes
        for (const auto& pair : all_classes_with_context) {
            std::shared_ptr<ClassDeclarationNode> class_decl = pair.first;
            const std::string& namespace_context = pair.second;
            
            // Construct the fully qualified class name
            std::string fq_class_name = namespace_context.empty() 
                                        ? class_decl->name->name 
                                        : namespace_context + "." + class_decl->name->name;
            
            // Pass the fully qualified name to the declaration method.
            declare_class_structure_and_signatures(class_decl, fq_class_name);
        }

        // PASS 2: Compile ALL method bodies (now all signatures are available for forward calls)
        for (const auto& pair : all_classes_with_context) {
            std::shared_ptr<ClassDeclarationNode> class_decl = pair.first;
            const std::string& namespace_context = pair.second;
            
            std::string fq_class_name = namespace_context.empty() 
                                        ? class_decl->name->name 
                                        : namespace_context + "." + class_decl->name->name;
                                        
            compile_all_method_bodies(class_decl, fq_class_name);
        }

        // PASS 3: Populate VTables (now all method bodies are compiled)
        for (const auto& pair : all_classes_with_context) {
            std::shared_ptr<ClassDeclarationNode> class_decl = pair.first;
            const std::string& namespace_context = pair.second;
            
            std::string fq_class_name = namespace_context.empty() 
                                        ? class_decl->name->name 
                                        : namespace_context + "." + class_decl->name->name;
                                        
            populate_vtable_for_class(fq_class_name);
        }

        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<NamespaceDeclarationNode> node)
    {
        // This method should no longer be called directly since we now use
        // a proper two-pass approach in visit(CompilationUnitNode).
        log_error("Old visit(NamespaceDeclarationNode) called - this should not happen with the new two-pass compilation approach", node->location);
        return nullptr;
    }

    void ScriptCompiler::visit(std::shared_ptr<ExternalMethodDeclarationNode> node)
    {
        // Check if the function is already declared (e.g., from the runtime bindings)
        if (llvmModule->getFunction(node->name->name)) {
            // This function already exists, likely from the runtime library.
            // We assume the signature is correct and do not re-declare it.
            // This prevents the creation of duplicate functions with suffixes (e.g., "print_int.1").
            return; 
        }

        if (!node->type.has_value())
            log_error("External method lacks return type.", node->location);
        llvm::Type *return_type = get_llvm_type(node->type.value());
        std::vector<llvm::Type *> param_types;
        for (const auto &param_node : node->parameters)
        {
            if (!param_node->type)
                log_error("External method param lacks type.", param_node->location);
            param_types.push_back(get_llvm_type(param_node->type));
        }
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_types, false);
        llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, node->name->name, llvmModule.get());
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ClassDeclarationNode> node)
    {
        // This method should no longer be called directly since we now use
        // a proper two-pass approach in visit(CompilationUnitNode).
        // If this is called, it means the compilation flow is incorrect.
        log_error("Old visit(ClassDeclarationNode) called - this should not happen with the new two-pass compilation approach", node->location);
        return nullptr;
    }

    llvm::Function *ScriptCompiler::visit_method_declaration(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name)
    {
        namedValues.clear();
        bool is_static = false;
        for (auto mod : node->modifiers)
            if (mod.first == ModifierKind::Static)
                is_static = true;
        if (!node->type.has_value())
            log_error("Method lacks return type.", node->location);
        llvm::Type *return_type = get_llvm_type(node->type.value());

        // Push function scope for the new method
        std::string func_name = class_name + "." + node->name->name;
        scope_manager->push_scope(ScopeType::Function, func_name);
        std::vector<llvm::Type *> param_llvm_types;
        const ClassTypeInfo *this_class_info = nullptr;
        if (!is_static)
        {
            auto cti_it = classTypeRegistry.find(class_name);
            if (cti_it == classTypeRegistry.end())
            {
                log_error("Class not found for instance method: " + class_name, node->location);
                return nullptr;
            }
            this_class_info = &cti_it->second;
            param_llvm_types.push_back(llvm::PointerType::getUnqual(*llvmContext));
        }
        for (const auto &param_node : node->parameters)
        {
            if (!param_node->type)
                log_error("Method param lacks type.", param_node->location);
            param_llvm_types.push_back(get_llvm_type(param_node->type));
        }
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());

        // Populate functionReturnClassInfoMap
        if (node->type.has_value())
        {
            // Assuming node->type.value() is a std::shared_ptr<TypeNameNode>
            std::shared_ptr<TypeNameNode> return_ast_type_node = node->type.value();
            if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&return_ast_type_node->name_segment))
            {
                if (auto identNode = *identNode_variant)
                {
                    auto cti_it = classTypeRegistry.find(identNode->name);
                    if (cti_it != classTypeRegistry.end())
                    {
                        functionReturnClassInfoMap[function] = &cti_it->second;
                    }
                }
            }
            // TODO: Handle QualifiedNameNode for return types if necessary for classes
        }

        currentFunction = function;
        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);
        auto llvm_arg_it = function->arg_begin();
        if (!is_static)
        {
            VariableInfo thisVarInfo;
            thisVarInfo.alloca = create_entry_block_alloca(function, "this", llvm_arg_it->getType());
            thisVarInfo.classInfo = this_class_info;
            llvmBuilder->CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
            namedValues["this"] = thisVarInfo;

            // Note: Field access in instance methods should use explicit this.field syntax
            // Simple field references like "value" should be parsed as "this.value"

            ++llvm_arg_it;
        }
        unsigned ast_param_idx = 0;
        for (; llvm_arg_it != function->arg_end(); ++llvm_arg_it, ++ast_param_idx)
        {
            VariableInfo paramVarInfo;
            paramVarInfo.alloca = create_entry_block_alloca(function, node->parameters[ast_param_idx]->name->name, llvm_arg_it->getType());
            paramVarInfo.declaredTypeNode = node->parameters[ast_param_idx]->type;
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&paramVarInfo.declaredTypeNode->name_segment))
            {
                auto cti_it = classTypeRegistry.find((*identNode)->name);
                if (cti_it != classTypeRegistry.end())
                    paramVarInfo.classInfo = &cti_it->second;
            }
            llvmBuilder->CreateStore(&*llvm_arg_it, paramVarInfo.alloca);
            namedValues[node->parameters[ast_param_idx]->name->name] = paramVarInfo;
        }
        if (node->body)
        {
            visit(node->body.value());
            if (!llvmBuilder->GetInsertBlock()->getTerminator())
            {
                // Pop function scope to clean up objects before return
                scope_manager->pop_scope();
                if (return_type->isVoidTy())
                    llvmBuilder->CreateRetVoid();
                else
                    log_error("Non-void function '" + func_name + "' missing return.", node->body.value()->location);
            }
            // Note: If there's already a terminator (return statement), the return visitor already popped the scope
        }
        else
        {
            log_error("Method '" + func_name + "' has no body.", node->location);
            if (return_type->isVoidTy() && !entry_block->getTerminator())
            {
                scope_manager->pop_scope();
                llvmBuilder->CreateRetVoid();
            }
        }
        return function;
    }

    llvm::Function *ScriptCompiler::declare_method_signature(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name)
    {
        bool is_static = false;
        for (auto mod : node->modifiers)
            if (mod.first == ModifierKind::Static)
                is_static = true;
        if (!node->type.has_value())
            log_error("Method lacks return type.", node->location);
        llvm::Type *return_type = get_llvm_type(node->type.value());

        std::string func_name = class_name + "." + node->name->name;
        std::vector<llvm::Type *> param_llvm_types;

        if (!is_static)
        {
            auto cti_it = classTypeRegistry.find(class_name);
            if (cti_it == classTypeRegistry.end())
            {
                log_error("Class not found for instance method: " + class_name, node->location);
                return nullptr;
            }
            param_llvm_types.push_back(llvm::PointerType::getUnqual(*llvmContext));
        }
        for (const auto &param_node : node->parameters)
        {
            if (!param_node->type)
                log_error("Method param lacks type.", param_node->location);
            param_llvm_types.push_back(get_llvm_type(param_node->type));
        }
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());

        // Populate functionReturnClassInfoMap for forward declarations
        if (node->type.has_value())
        {
            std::shared_ptr<TypeNameNode> return_ast_type_node = node->type.value();
            if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&return_ast_type_node->name_segment))
            {
                if (auto identNode = *identNode_variant)
                {
                    auto cti_it = classTypeRegistry.find(identNode->name);
                    if (cti_it != classTypeRegistry.end())
                    {
                        functionReturnClassInfoMap[function] = &cti_it->second;
                    }
                }
            }
        }

        return function;
    }

    void ScriptCompiler::compile_method_body(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name)
    {
        namedValues.clear();
        bool is_static = false;
        for (auto mod : node->modifiers)
            if (mod.first == ModifierKind::Static)
                is_static = true;

        std::string func_name = class_name + "." + node->name->name;
        llvm::Function *function = llvmModule->getFunction(func_name);
        if (!function)
        {
            log_error("Function signature not found during body compilation: " + func_name, node->location);
            return;
        }

        // Push function scope for the method body compilation
        scope_manager->push_scope(ScopeType::Function, func_name);
        const ClassTypeInfo *this_class_info = nullptr;
        if (!is_static)
        {
            auto cti_it = classTypeRegistry.find(class_name);
            if (cti_it == classTypeRegistry.end())
            {
                log_error("Class not found for instance method: " + class_name, node->location);
                return;
            }
            this_class_info = &cti_it->second;
        }

        currentFunction = function;
        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);
        auto llvm_arg_it = function->arg_begin();
        if (!is_static)
        {
            VariableInfo thisVarInfo;
            thisVarInfo.alloca = create_entry_block_alloca(function, "this", llvm_arg_it->getType());
            thisVarInfo.classInfo = this_class_info;
            llvmBuilder->CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
            namedValues["this"] = thisVarInfo;
            ++llvm_arg_it;
        }
        unsigned ast_param_idx = 0;
        for (; llvm_arg_it != function->arg_end(); ++llvm_arg_it, ++ast_param_idx)
        {
            VariableInfo paramVarInfo;
            paramVarInfo.alloca = create_entry_block_alloca(function, node->parameters[ast_param_idx]->name->name, llvm_arg_it->getType());
            paramVarInfo.declaredTypeNode = node->parameters[ast_param_idx]->type;
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&paramVarInfo.declaredTypeNode->name_segment))
            {
                auto cti_it = classTypeRegistry.find((*identNode)->name);
                if (cti_it != classTypeRegistry.end())
                    paramVarInfo.classInfo = &cti_it->second;
            }
            llvmBuilder->CreateStore(&*llvm_arg_it, paramVarInfo.alloca);
            namedValues[node->parameters[ast_param_idx]->name->name] = paramVarInfo;
        }
        if (node->body)
        {
            visit(node->body.value());
            if (!llvmBuilder->GetInsertBlock()->getTerminator())
            {
                scope_manager->pop_scope();
                llvm::Type *return_type = function->getReturnType();
                if (return_type->isVoidTy())
                    llvmBuilder->CreateRetVoid();
                else
                    log_error("Non-void function '" + func_name + "' missing return.", node->body.value()->location);
            }
        }
        else
        {
            log_error("Method '" + func_name + "' has no body.", node->location);
            if (function->getReturnType()->isVoidTy() && !llvmBuilder->GetInsertBlock()->getTerminator())
            {
                scope_manager->pop_scope();
                llvmBuilder->CreateRetVoid();
            }
        }
    }

    llvm::Function *ScriptCompiler::declare_constructor_signature(std::shared_ptr<ConstructorDeclarationNode> node, const std::string &class_name)
    {
        std::string func_name = class_name + ".%ctor";
        llvm::Type *return_type = llvm::Type::getVoidTy(*llvmContext);
        std::vector<llvm::Type *> param_llvm_types;

        auto cti_it = classTypeRegistry.find(class_name);
        if (cti_it == classTypeRegistry.end())
        {
            log_error("Class not found for constructor: " + class_name, node->location);
            return nullptr;
        }
        param_llvm_types.push_back(llvm::PointerType::getUnqual(*llvmContext));

        for (const auto &param_node : node->parameters)
        {
            if (!param_node->type)
            {
                log_error("Constructor parameter lacks type in " + class_name, param_node->location);
                return nullptr;
            }
            param_llvm_types.push_back(get_llvm_type(param_node->type));
        }
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());
        return function;
    }

    void ScriptCompiler::compile_constructor_body(std::shared_ptr<ConstructorDeclarationNode> node, const std::string &class_name)
    {
        namedValues.clear();
        std::string func_name = class_name + ".%ctor";
        llvm::Function *function = llvmModule->getFunction(func_name);
        if (!function)
        {
            log_error("Constructor signature not found during body compilation: " + func_name, node->location);
            return;
        }

        const ClassTypeInfo *this_class_info = nullptr;
        auto cti_it = classTypeRegistry.find(class_name);
        if (cti_it == classTypeRegistry.end())
        {
            log_error("Class not found for constructor: " + class_name, node->location);
            return;
        }
        this_class_info = &cti_it->second;

        // Push constructor scope
        scope_manager->push_scope(ScopeType::Function, func_name);

        currentFunction = function;
        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);
        auto llvm_arg_it = function->arg_begin();
        VariableInfo thisVarInfo;
        thisVarInfo.alloca = create_entry_block_alloca(function, "this.ctor.arg", llvm_arg_it->getType());
        thisVarInfo.classInfo = this_class_info;
        llvmBuilder->CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
        namedValues["this"] = thisVarInfo;

        ++llvm_arg_it;
        unsigned ast_param_idx = 0;
        for (; llvm_arg_it != function->arg_end(); ++llvm_arg_it, ++ast_param_idx)
        {
            if (ast_param_idx >= node->parameters.size())
            {
                log_error("LLVM argument count mismatch for constructor " + func_name, node->location);
                return;
            }
            const auto &ast_param = node->parameters[ast_param_idx];
            VariableInfo paramVarInfo;
            paramVarInfo.alloca = create_entry_block_alloca(function, ast_param->name->name, llvm_arg_it->getType());
            paramVarInfo.declaredTypeNode = ast_param->type;
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&paramVarInfo.declaredTypeNode->name_segment))
            {
                auto cti_param_it = classTypeRegistry.find((*identNode)->name);
                if (cti_param_it != classTypeRegistry.end())
                    paramVarInfo.classInfo = &cti_param_it->second;
            }
            llvmBuilder->CreateStore(&*llvm_arg_it, paramVarInfo.alloca);
            namedValues[ast_param->name->name] = paramVarInfo;
        }
        if (node->body)
        {
            visit(node->body.value());
            if (!llvmBuilder->GetInsertBlock()->getTerminator())
            {
                scope_manager->pop_scope();
                llvmBuilder->CreateRetVoid();
            }
        }
        else
        {
            log_error("Constructor '" + func_name + "' has no body.", node->location);
            if (!llvmBuilder->GetInsertBlock()->getTerminator())
            {
                scope_manager->pop_scope();
                llvmBuilder->CreateRetVoid();
            }
        }
    }

    llvm::Function *ScriptCompiler::declare_destructor_signature(std::shared_ptr<DestructorDeclarationNode> node, const std::string &class_name)
    {
        std::string func_name = class_name + ".%dtor";
        llvm::Type *return_type = llvm::Type::getVoidTy(*llvmContext);
        std::vector<llvm::Type *> param_llvm_types = {llvm::PointerType::getUnqual(*llvmContext)};
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());
        return function;
    }

    llvm::Function *ScriptCompiler::compile_destructor_body(std::shared_ptr<DestructorDeclarationNode> node, const std::string &class_name)
    {
        namedValues.clear();
        std::string func_name = class_name + ".%dtor";
        llvm::Function *function = llvmModule->getFunction(func_name);
        if (!function)
        {
            log_error("Destructor signature not found during body compilation: " + func_name, node->location);
            return nullptr;
        }

        auto cti_it = classTypeRegistry.find(class_name);
        if (cti_it == classTypeRegistry.end())
        {
            log_error("Class not found for destructor: " + class_name, node->location);
            return nullptr;
        }
        const ClassTypeInfo *this_class_info = &cti_it->second;

        // Push destructor scope
        scope_manager->push_scope(ScopeType::Function, func_name);

        currentFunction = function;
        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);
        auto llvm_arg_it = function->arg_begin();
        VariableInfo thisVarInfo;
        thisVarInfo.alloca = create_entry_block_alloca(function, "this.dtor.arg", llvm_arg_it->getType());
        thisVarInfo.classInfo = this_class_info;
        llvmBuilder->CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
        namedValues["this"] = thisVarInfo;

        // Set up field access for the destructor
        if (this_class_info && this_class_info->fieldsType)
        {
            llvm::Value *this_fields_ptr = llvmBuilder->CreateLoad(llvm_arg_it->getType(), thisVarInfo.alloca, "this.fields.dtor");
            for (unsigned i = 0; i < this_class_info->field_names_in_order.size(); ++i)
            {
                const std::string &field_name = this_class_info->field_names_in_order[i];
                llvm::Type *field_llvm_type = this_class_info->fieldsType->getElementType(i);

                llvm::Value *field_ptr = llvmBuilder->CreateStructGEP(this_class_info->fieldsType, this_fields_ptr, i, field_name + ".ptr.dtor");

                VariableInfo fieldVarInfo;
                fieldVarInfo.alloca = create_entry_block_alloca(function, field_name + ".dtor.access", field_llvm_type);
                fieldVarInfo.declaredTypeNode = this_class_info->field_ast_types[i];

                llvm::Value *field_val = llvmBuilder->CreateLoad(field_llvm_type, field_ptr, field_name + ".val.dtor");
                llvmBuilder->CreateStore(field_val, fieldVarInfo.alloca);

                if (field_llvm_type->isPointerTy() && fieldVarInfo.declaredTypeNode)
                {
                    if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&fieldVarInfo.declaredTypeNode->name_segment))
                    {
                        auto field_cti_it = classTypeRegistry.find((*identNode)->name);
                        if (field_cti_it != classTypeRegistry.end())
                        {
                            fieldVarInfo.classInfo = &field_cti_it->second;
                        }
                    }
                }

                namedValues[field_name] = fieldVarInfo;
            }
        }

        if (node->body)
        {
            visit(node->body.value());
        }
        if (!llvmBuilder->GetInsertBlock()->getTerminator())
        {
            scope_manager->pop_scope();
            llvmBuilder->CreateRetVoid();
        }
        if (llvm::verifyFunction(*function, &llvm::errs()))
        {
            log_error("Destructor function '" + func_name + "' verification failed. Dumping IR.", node->location);
            function->print(llvm::errs());
        }
        return function;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<StatementNode> node) { return visit(std::static_pointer_cast<AstNode>(node)); }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<BlockStatementNode> node)
    {
        // Push block scope for proper object lifecycle management
        scope_manager->push_scope(ScopeType::Block, "block");

        llvm::Value *last_val = nullptr;
        for (const auto &stmt : node->statements)
        {
            if (llvmBuilder->GetInsertBlock()->getTerminator())
                break;
            last_val = visit(stmt);
        }

        // Pop block scope - this will automatically clean up any objects created in this scope
        scope_manager->pop_scope();

        return last_val;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<LocalVariableDeclarationStatementNode> node)
    {
        llvm::Type *var_llvm_type = get_llvm_type(node->type);
        const ClassTypeInfo *var_static_class_info = nullptr;
        if (var_llvm_type->isPointerTy())
        {
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment))
            {
                auto cti_it = classTypeRegistry.find((*identNode)->name);
                if (cti_it != classTypeRegistry.end())
                    var_static_class_info = &cti_it->second;
            }
        }
        for (const auto &declarator : node->declarators)
        {
            VariableInfo varInfo;
            varInfo.alloca = create_entry_block_alloca(currentFunction, declarator->name->name, var_llvm_type);
            varInfo.classInfo = var_static_class_info;
            varInfo.declaredTypeNode = node->type;
            namedValues[declarator->name->name] = varInfo;
            if (declarator->initializer)
            {
                ExpressionVisitResult init_res = visit(declarator->initializer.value());
                llvm::Value *init_val = init_res.value;
                const ClassTypeInfo *init_val_class_info = init_res.classInfo;
                if (!init_val)
                {
                    log_error("Initializer for " + declarator->name->name + " failed.", declarator->initializer.value()->location);
                    continue;
                }
                if (init_val->getType() != var_llvm_type)
                {
                    log_error("LLVM type mismatch for initializer of " + declarator->name->name, declarator->initializer.value()->location);
                }
                // Type compatibility already validated by semantic analyzer
                // No need for inheritance hierarchy checking here
                // CRITICAL ARC FIX: Add proper retain logic for variable initialization
                // This ensures that `TestObject copy = original;` properly retains the source object
                // But skip retain for new expressions as they already have correct ref_count
                if (var_static_class_info && var_static_class_info->fieldsType && init_val->getType()->isPointerTy())
                {
                    // Check if the initializer is a new expression (ObjectCreationExpression)
                    bool is_new_expression = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(declarator->initializer.value()) != nullptr;

                    if (!is_new_expression)
                    {
                        // Only retain if this is NOT a new expression
                        llvm::Value *init_object_header = nullptr;
                        if (init_res.header_ptr)
                        {
                            init_object_header = init_res.header_ptr;
                        }
                        else
                        {
                            init_object_header = getHeaderPtrFromFieldsPtr(init_val, var_static_class_info->fieldsType);
                        }
                        if (init_object_header)
                        {
                            llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_retain"), {init_object_header});
                        }
                    }
                }

                llvmBuilder->CreateStore(init_val, varInfo.alloca);

                // Check if this is a declared type name to exclude built-in types like 'string'
                std::string declared_type_name;
                if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment))
                {
                    declared_type_name = (*identNode)->name;
                }

                // UNIFIED ARC MANAGEMENT: Use only scope manager, remove dual systems
                // Register ARC objects with scope manager for consistent cleanup
                if (var_static_class_info && var_static_class_info->fieldsType &&
                    init_val->getType()->isPointerTy() && declared_type_name != "string")
                {

                    // Register with scope manager for unified ARC management
                    scope_manager->register_arc_managed_object(
                        varInfo.alloca,
                        var_static_class_info,
                        declarator->name->name);
                }
            }
        }
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ExpressionStatementNode> node)
    {
        if (!node->expression)
        {
            log_error("ExpressionStatementNode has no expression.", node->location);
            return nullptr;
        }
        return visit(node->expression).value;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<IfStatementNode> node)
    {
        ExpressionVisitResult cond_res = visit(node->condition);
        if (!cond_res.value)
        {
            log_error("If statement condition is null.", node->condition->location);
            return nullptr;
        }
        llvm::Value *cond_val = cond_res.value;
        if (!cond_val->getType()->isIntegerTy(1))
        {
            cond_val = llvmBuilder->CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "tobool");
        }
        llvm::Function *TheFunction = llvmBuilder->GetInsertBlock()->getParent();
        llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*llvmContext, "then", TheFunction);
        llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*llvmContext, "else");

        llvmBuilder->CreateCondBr(cond_val, ThenBB, ElseBB);

        // Compile then branch
        llvmBuilder->SetInsertPoint(ThenBB);
        visit(node->thenStatement);
        bool then_has_terminator = llvmBuilder->GetInsertBlock()->getTerminator() != nullptr;
        ThenBB = llvmBuilder->GetInsertBlock();

        // Compile else branch
        TheFunction->insert(TheFunction->end(), ElseBB);
        llvmBuilder->SetInsertPoint(ElseBB);
        if (node->elseStatement.has_value())
        {
            visit(node->elseStatement.value());
        }
        bool else_has_terminator = llvmBuilder->GetInsertBlock()->getTerminator() != nullptr;
        ElseBB = llvmBuilder->GetInsertBlock();

        // Only create and use merge block if at least one branch doesn't have a terminator
        if (!then_has_terminator || !else_has_terminator)
        {
            llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*llvmContext, "ifcont");

            // Add branches to merge block from branches that don't have terminators
            if (!then_has_terminator)
            {
                llvmBuilder->SetInsertPoint(ThenBB);
                llvmBuilder->CreateBr(MergeBB);
            }
            if (!else_has_terminator)
            {
                llvmBuilder->SetInsertPoint(ElseBB);
                llvmBuilder->CreateBr(MergeBB);
            }

            // Insert merge block and set as current insert point
            TheFunction->insert(TheFunction->end(), MergeBB);
            llvmBuilder->SetInsertPoint(MergeBB);
        }
        // If both branches have terminators, don't create a merge block at all
        // The insert point will be invalid, but that's okay since control flow has ended

        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<WhileStatementNode> node)
    {
        ExpressionVisitResult cond_res = visit(node->condition);
        if (!cond_res.value)
        {
            log_error("While statement condition is null.", node->condition->location);
            return nullptr;
        }

        llvm::Function *function = llvmBuilder->GetInsertBlock()->getParent();
        llvm::BasicBlock *condBB = llvm::BasicBlock::Create(*llvmContext, "while.cond", function);
        llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(*llvmContext, "while.body");
        llvm::BasicBlock *exitBB = llvm::BasicBlock::Create(*llvmContext, "while.exit");

        // Jump to condition check
        llvmBuilder->CreateBr(condBB);

        // Condition block
        llvmBuilder->SetInsertPoint(condBB);
        ExpressionVisitResult loop_cond_res = visit(node->condition);
        llvm::Value *cond_val = loop_cond_res.value;
        if (!cond_val->getType()->isIntegerTy(1))
        {
            cond_val = llvmBuilder->CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "tobool");
        }
        llvmBuilder->CreateCondBr(cond_val, bodyBB, exitBB);

        // Body block
        function->insert(function->end(), bodyBB);
        llvmBuilder->SetInsertPoint(bodyBB);

        // Push loop context for break/continue
        loop_context_stack.push_back(LoopContext(exitBB, condBB));

        visit(node->body);

        // Pop loop context
        loop_context_stack.pop_back();

        if (!llvmBuilder->GetInsertBlock()->getTerminator())
        {
            llvmBuilder->CreateBr(condBB); // Loop back to condition
        }

        // Exit block
        function->insert(function->end(), exitBB);
        llvmBuilder->SetInsertPoint(exitBB);

        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ForStatementNode> node)
    {
        llvm::Function *function = llvmBuilder->GetInsertBlock()->getParent();

        // Handle initializer
        if (std::holds_alternative<std::shared_ptr<LocalVariableDeclarationStatementNode>>(node->initializers))
        {
            visit(std::get<std::shared_ptr<LocalVariableDeclarationStatementNode>>(node->initializers));
        }
        else if (std::holds_alternative<std::vector<std::shared_ptr<ExpressionNode>>>(node->initializers))
        {
            for (auto &init_expr : std::get<std::vector<std::shared_ptr<ExpressionNode>>>(node->initializers))
            {
                visit(init_expr);
            }
        }

        // Create basic blocks
        llvm::BasicBlock *condBB = llvm::BasicBlock::Create(*llvmContext, "for.cond", function);
        llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(*llvmContext, "for.body");
        llvm::BasicBlock *incBB = llvm::BasicBlock::Create(*llvmContext, "for.inc");
        llvm::BasicBlock *exitBB = llvm::BasicBlock::Create(*llvmContext, "for.exit");

        // Jump to condition
        llvmBuilder->CreateBr(condBB);

        // Condition block
        llvmBuilder->SetInsertPoint(condBB);
        if (node->condition)
        {
            ExpressionVisitResult cond_res = visit(node->condition.value());
            llvm::Value *cond_val = cond_res.value;
            if (!cond_val->getType()->isIntegerTy(1))
            {
                cond_val = llvmBuilder->CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "tobool");
            }
            llvmBuilder->CreateCondBr(cond_val, bodyBB, exitBB);
        }
        else
        {
            // No condition means infinite loop (unless broken)
            llvmBuilder->CreateBr(bodyBB);
        }

        // Body block
        function->insert(function->end(), bodyBB);
        llvmBuilder->SetInsertPoint(bodyBB);

        // Push loop context for break/continue
        loop_context_stack.push_back(LoopContext(exitBB, incBB));

        visit(node->body);

        // Pop loop context
        loop_context_stack.pop_back();

        if (!llvmBuilder->GetInsertBlock()->getTerminator())
        {
            llvmBuilder->CreateBr(incBB);
        }

        // Increment block
        function->insert(function->end(), incBB);
        llvmBuilder->SetInsertPoint(incBB);
        for (auto &inc_expr : node->incrementors)
        {
            visit(inc_expr);
        }
        llvmBuilder->CreateBr(condBB); // Loop back to condition

        // Exit block
        function->insert(function->end(), exitBB);
        llvmBuilder->SetInsertPoint(exitBB);

        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ReturnStatementNode> node)
    {
        // Generate the return value first
        llvm::Value *return_value = nullptr;
        if (node->expression)
        {
            ExpressionVisitResult ret_res = visit(node->expression.value());
            if (!ret_res.value)
            {
                log_error("Return expression compiled to null.", node->expression.value()->location);
                return nullptr;
            }
            // Return type compatibility already validated by semantic analyzer
            return_value = ret_res.value;
        }
        else
        {
            if (!currentFunction->getReturnType()->isVoidTy())
            {
                log_error("Non-void function missing return value.", node->location);
            }
        }

        // Clean up function scope before return (handles all cleanup via scope manager)
        scope_manager->pop_scope();

        // Generate the return instruction
        if (return_value)
        {
            llvmBuilder->CreateRet(return_value);
        }
        else
        {
            llvmBuilder->CreateRetVoid();
        }

        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<BreakStatementNode> node)
    {
        if (loop_context_stack.empty())
        {
            log_error("'break' statement used outside of loop.", node->location);
            return nullptr;
        }

        const LoopContext &current_loop = loop_context_stack.back();
        llvmBuilder->CreateBr(current_loop.exit_block);
        return nullptr;
    }

    llvm::Value *ScriptCompiler::visit(std::shared_ptr<ContinueStatementNode> node)
    {
        if (loop_context_stack.empty())
        {
            log_error("'continue' statement used outside of loop.", node->location);
            return nullptr;
        }

        // CRITICAL: Clean up scope BEFORE creating the terminator instruction
        // This ensures any object destructors are called before the continue jump
        scope_manager->cleanup_current_scope_early();

        const LoopContext &current_loop = loop_context_stack.back();
        llvmBuilder->CreateBr(current_loop.continue_block);
        return nullptr;
    }

    llvm::Function *ScriptCompiler::visit(std::shared_ptr<ConstructorDeclarationNode> node, const std::string &class_name)
    {
        namedValues.clear();
        std::string func_name = class_name + ".%ctor";
        llvm::Type *return_type = llvm::Type::getVoidTy(*llvmContext);
        std::vector<llvm::Type *> param_llvm_types;
        const ClassTypeInfo *this_class_info = nullptr;
        auto cti_it = classTypeRegistry.find(class_name);
        if (cti_it == classTypeRegistry.end())
        {
            log_error("Class not found for constructor: " + class_name, node->location);
            return nullptr;
        }
        this_class_info = &cti_it->second;
        param_llvm_types.push_back(llvm::PointerType::getUnqual(*llvmContext));

        // Push constructor scope
        scope_manager->push_scope(ScopeType::Function, func_name);

        for (const auto &param_node : node->parameters)
        {
            if (!param_node->type)
            {
                log_error("Constructor parameter lacks type in " + class_name, param_node->location);
                return nullptr;
            }
            param_llvm_types.push_back(get_llvm_type(param_node->type));
        }
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());
        currentFunction = function;
        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);
        auto llvm_arg_it = function->arg_begin();
        VariableInfo thisVarInfo;
        thisVarInfo.alloca = create_entry_block_alloca(function, "this.ctor.arg", llvm_arg_it->getType());
        thisVarInfo.classInfo = this_class_info;
        llvmBuilder->CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
        namedValues["this"] = thisVarInfo;

        // Note: Field access in constructors should use explicit this.field syntax
        // Simple field assignments like "value = val" should be parsed as "this.value = val"

        ++llvm_arg_it;
        unsigned ast_param_idx = 0;
        for (; llvm_arg_it != function->arg_end(); ++llvm_arg_it, ++ast_param_idx)
        {
            if (ast_param_idx >= node->parameters.size())
            {
                log_error("LLVM argument count mismatch for constructor " + func_name, node->location);
                return function;
            }
            const auto &ast_param = node->parameters[ast_param_idx];
            VariableInfo paramVarInfo;
            paramVarInfo.alloca = create_entry_block_alloca(function, ast_param->name->name, llvm_arg_it->getType());
            paramVarInfo.declaredTypeNode = ast_param->type;
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&paramVarInfo.declaredTypeNode->name_segment))
            {
                auto cti_param_it = classTypeRegistry.find((*identNode)->name);
                if (cti_param_it != classTypeRegistry.end())
                    paramVarInfo.classInfo = &cti_param_it->second;
            }
            llvmBuilder->CreateStore(&*llvm_arg_it, paramVarInfo.alloca);
            namedValues[ast_param->name->name] = paramVarInfo;
        }
        if (node->body)
        {
            visit(node->body.value());
            if (!llvmBuilder->GetInsertBlock()->getTerminator())
            {
                // Use scope manager instead of old cleanup system
                scope_manager->pop_scope();
                llvmBuilder->CreateRetVoid();
            }
        }
        else
        {
            log_error("Constructor '" + func_name + "' has no body.", node->location);
            if (!entry_block->getTerminator())
            {
                scope_manager->pop_scope();
                llvmBuilder->CreateRetVoid();
            }
        }
        return function;
    }

    llvm::Function *ScriptCompiler::visit(std::shared_ptr<DestructorDeclarationNode> node, const std::string &class_name)
    {
        namedValues.clear();
        std::string func_name = class_name + ".%dtor";
        llvm::Type *return_type = llvm::Type::getVoidTy(*llvmContext);
        std::vector<llvm::Type *> param_llvm_types = {llvm::PointerType::getUnqual(*llvmContext)};
        llvm::FunctionType *func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
        llvm::Function *function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, llvmModule.get());
        currentFunction = function;
        auto cti_it = classTypeRegistry.find(class_name);
        if (cti_it == classTypeRegistry.end())
        {
            log_error("Class not found for destructor: " + class_name, node->location);
            return nullptr;
        }
        const ClassTypeInfo *this_class_info = &cti_it->second;

        // Push destructor scope (though destructors typically don't create many local objects)
        scope_manager->push_scope(ScopeType::Function, func_name);

        llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*llvmContext, "entry", function);
        llvmBuilder->SetInsertPoint(entry_block);
        auto llvm_arg_it = function->arg_begin();
        VariableInfo thisVarInfo;
        thisVarInfo.alloca = create_entry_block_alloca(function, "this.dtor.arg", llvm_arg_it->getType());
        thisVarInfo.classInfo = this_class_info;
        llvmBuilder->CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
        namedValues["this"] = thisVarInfo;

        // Set up field access for the destructor - add each field to namedValues for direct access
        if (this_class_info && this_class_info->fieldsType)
        {
            llvm::Value *this_fields_ptr = llvmBuilder->CreateLoad(llvm_arg_it->getType(), thisVarInfo.alloca, "this.fields.dtor");
            for (unsigned i = 0; i < this_class_info->field_names_in_order.size(); ++i)
            {
                const std::string &field_name = this_class_info->field_names_in_order[i];
                llvm::Type *field_llvm_type = this_class_info->fieldsType->getElementType(i);

                // Create a pseudo-alloca for the field that points directly to the struct member
                llvm::Value *field_ptr = llvmBuilder->CreateStructGEP(this_class_info->fieldsType, this_fields_ptr, i, field_name + ".ptr.dtor");

                VariableInfo fieldVarInfo;
                fieldVarInfo.alloca = create_entry_block_alloca(function, field_name + ".dtor.access", field_llvm_type);
                fieldVarInfo.declaredTypeNode = this_class_info->field_ast_types[i];

                // Load the field value and store it in the pseudo-alloca for access
                llvm::Value *field_val = llvmBuilder->CreateLoad(field_llvm_type, field_ptr, field_name + ".val.dtor");
                llvmBuilder->CreateStore(field_val, fieldVarInfo.alloca);

                // Check if this is an object field for class info
                if (field_llvm_type->isPointerTy() && fieldVarInfo.declaredTypeNode)
                {
                    if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&fieldVarInfo.declaredTypeNode->name_segment))
                    {
                        auto field_cti_it = classTypeRegistry.find((*identNode)->name);
                        if (field_cti_it != classTypeRegistry.end())
                        {
                            fieldVarInfo.classInfo = &field_cti_it->second;
                        }
                    }
                }

                namedValues[field_name] = fieldVarInfo;
            }
        }

        if (node->body)
        {
            visit(node->body.value());
        }
        // NOTE: Field cleanup is now handled by ARC when the object's ref count reaches zero
        // Destructors should only contain user-defined cleanup code, not automatic field cleanup
        // This prevents race conditions between manual field cleanup and scope management
        if (!llvmBuilder->GetInsertBlock()->getTerminator())
        {
            // Use scope manager instead of old cleanup system
            scope_manager->pop_scope();
            llvmBuilder->CreateRetVoid();
        }
        if (llvm::verifyFunction(*function, &llvm::errs()))
        {
            log_error("Destructor function '" + func_name + "' verification failed. Dumping IR.", node->location);
            function->print(llvm::errs());
        }
        return function;
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<ExpressionNode> node)
    {
        if (!node)
            log_error("Attempted to visit a null ExpressionNode.");
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
        if (auto specificNode = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node))
            return visit(specificNode);
        if (auto specificNode = std::dynamic_pointer_cast<ParenthesizedExpressionNode>(node))
            return visit(specificNode);
        log_error("Unhandled ExpressionNode type: " + std::string(typeid(*node).name()), node->location);
        return ExpressionVisitResult(nullptr);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<LiteralExpressionNode> node)
    {
        llvm::Value *val = nullptr;
        const ClassTypeInfo *ci = nullptr;
        switch (node->kind)
        {
        case LiteralKind::Integer:
            try
            {
                val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvmContext), static_cast<int32_t>(std::stoll(node->valueText)), true);
            }
            catch (const std::exception &e)
            {
                log_error("Invalid int literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::Long:
            try
            {
                val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), static_cast<int64_t>(std::stoll(node->valueText)), true);
            }
            catch (const std::exception &e)
            {
                log_error("Invalid long literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::Float:
            try
            {
                val = llvm::ConstantFP::get(llvm::Type::getFloatTy(*llvmContext), llvm::APFloat(std::stof(node->valueText)));
            }
            catch (const std::exception &e)
            {
                log_error("Invalid float literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::Double:
            try
            {
                val = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*llvmContext), llvm::APFloat(std::stod(node->valueText)));
            }
            catch (const std::exception &e)
            {
                log_error("Invalid double literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::Boolean:
            val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*llvmContext), (node->valueText == "true"));
            break;
        case LiteralKind::Char:
            if (node->valueText.length() == 1)
            {
                val = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*llvmContext), node->valueText[0]);
            }
            else
            {
                log_error("Invalid char literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::String:
        {
            llvm::Constant *strConst = llvm::ConstantDataArray::getString(*llvmContext, node->valueText, true);
            auto *globalVar = new llvm::GlobalVariable(*llvmModule, strConst->getType(), true, llvm::GlobalValue::PrivateLinkage, strConst, ".str");
            globalVar->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
            globalVar->setAlignment(llvm::MaybeAlign(1));
            llvm::Value *charPtr = llvmBuilder->CreateBitCast(globalVar, llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llvmContext)));
            llvm::Value *lenVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), node->valueText.length());
            llvm::Function *newStrFunc = llvmModule->getFunction("Mycelium_String_new_from_literal");
            if (!newStrFunc)
            {
                log_error("Runtime Mycelium_String_new_from_literal not found.", node->location);
                return ExpressionVisitResult(nullptr);
            }
            val = llvmBuilder->CreateCall(newStrFunc, {charPtr, lenVal}, "new_mycelium_str");
        }
        break;
        case LiteralKind::Null:
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
        std::string name = node->identifier->name;

        // 1. Check for local variables or parameters
        auto it = namedValues.find(name);
        if (it != namedValues.end())
        {
            const VariableInfo &varInfo = it->second;
            llvm::Value *loaded_val = llvmBuilder->CreateLoad(varInfo.alloca->getAllocatedType(), varInfo.alloca, name.c_str());
            return ExpressionVisitResult(loaded_val, varInfo.classInfo);
        }

        // 2. Check for implicit 'this' field access
        auto this_it = namedValues.find("this");
        if (this_it != namedValues.end() && this_it->second.classInfo)
        {
            const ClassTypeInfo *class_info = this_it->second.classInfo;
            auto field_it = class_info->field_indices.find(name);
            if (field_it != class_info->field_indices.end())
            {
                auto this_expr = std::make_shared<ThisExpressionNode>();
                this_expr->thisKeyword = std::make_shared<TokenNode>();
                this_expr->location = node->location;

                auto member_access = std::make_shared<MemberAccessExpressionNode>();
                member_access->target = this_expr;
                member_access->memberName = node->identifier;
                member_access->location = node->location;
                return visit(member_access);
            }
        }

        // 3. Check for a class name
        if (symbolTable)
        {
            const auto* class_symbol = symbolTable->find_class(name);
            if (class_symbol)
            {
                auto cti_it = classTypeRegistry.find(name);
                if (cti_it != classTypeRegistry.end())
                {
                    ExpressionVisitResult res;
                    res.classInfo = &cti_it->second;
                    res.is_static_type = true;
                    res.resolved_path = name;
                    return res;
                }
            }
        }

        // 4. Check for a namespace
        if (symbolTable)
        {
            const auto& all_classes = symbolTable->get_classes();
            for (const auto& [class_name, class_symbol] : all_classes)
            {
                if (class_name.rfind(name + ".", 0) == 0)
                { 
                    ExpressionVisitResult res;
                    res.resolved_path = name;
                    return res; 
                }
            }
        }

        // All symbol resolution validated by semantic analyzer
        // This should never be reached with valid SemanticIR
        return ExpressionVisitResult(nullptr);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<BinaryExpressionNode> node)
    {
        ExpressionVisitResult L_res = visit(node->left);
        ExpressionVisitResult R_res = visit(node->right);
        llvm::Value *L = L_res.value;
        llvm::Value *R = R_res.value;
        if (!L || !R)
        {
            log_error("One or both operands of binary expression are null.", node->location);
            return ExpressionVisitResult(nullptr);
        }
        llvm::Type *LType = L->getType();
        llvm::Type *RType = R->getType();
        if (node->opKind == BinaryOperatorKind::Add && LType == getMyceliumStringPtrTy() && RType == getMyceliumStringPtrTy())
        {
            llvm::Function *concatFunc = llvmModule->getFunction("Mycelium_String_concat");
            if (!concatFunc)
            {
                log_error("Runtime Mycelium_String_concat not found.", node->location);
                return ExpressionVisitResult(nullptr);
            }
            llvm::Value *result_str_ptr = llvmBuilder->CreateCall(concatFunc, {L, R}, "concat_str");
            return ExpressionVisitResult(result_str_ptr, nullptr);
        }
        if (node->opKind == BinaryOperatorKind::Add)
        {
            if (LType == getMyceliumStringPtrTy() && RType->isIntegerTy(32))
            {
                llvm::Function *fromIntFunc = llvmModule->getFunction("Mycelium_String_from_int");
                if (!fromIntFunc)
                {
                    log_error("Mycelium_String_from_int not found", node->right->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *r_as_str = llvmBuilder->CreateCall(fromIntFunc, {R}, "int_to_str_tmp");
                llvm::Function *concatFunc = llvmModule->getFunction("Mycelium_String_concat");
                if (!concatFunc)
                {
                    log_error("Mycelium_String_concat not found", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result_str_ptr = llvmBuilder->CreateCall(concatFunc, {L, r_as_str}, "concat_str_int");
                return ExpressionVisitResult(result_str_ptr, nullptr);
            }
            else if (LType == getMyceliumStringPtrTy() && RType->isIntegerTy(1))
            {
                // Case: string + bool
                llvm::Function *fromBoolFunc = llvmModule->getFunction("Mycelium_String_from_bool");
                if (!fromBoolFunc)
                {
                    log_error("Mycelium_String_from_bool not found", node->right->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *r_as_str = llvmBuilder->CreateCall(fromBoolFunc, {R}, "bool_to_str_tmp");
                llvm::Function *concatFunc = llvmModule->getFunction("Mycelium_String_concat");
                if (!concatFunc)
                {
                    log_error("Mycelium_String_concat not found", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result_str_ptr = llvmBuilder->CreateCall(concatFunc, {L, r_as_str}, "concat_str_bool");
                return ExpressionVisitResult(result_str_ptr, nullptr);
            }
            else if (LType->isIntegerTy(1) && RType == getMyceliumStringPtrTy())
            {
                // Case: bool + string
                llvm::Function *fromBoolFunc = llvmModule->getFunction("Mycelium_String_from_bool");
                if (!fromBoolFunc)
                {
                    log_error("Mycelium_String_from_bool not found", node->left->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *l_as_str = llvmBuilder->CreateCall(fromBoolFunc, {L}, "bool_to_str_tmp");
                llvm::Function *concatFunc = llvmModule->getFunction("Mycelium_String_concat");
                if (!concatFunc)
                {
                    log_error("Mycelium_String_concat not found", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result_str_ptr = llvmBuilder->CreateCall(concatFunc, {l_as_str, R}, "concat_bool_str");
                return ExpressionVisitResult(result_str_ptr, nullptr);
            }
            else if (LType->isIntegerTy(32) && RType == getMyceliumStringPtrTy())
            {
                // Case: int + string (symmetric to string + int)
                llvm::Function *fromIntFunc = llvmModule->getFunction("Mycelium_String_from_int");
                if (!fromIntFunc)
                {
                    log_error("Mycelium_String_from_int not found", node->left->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *l_as_str = llvmBuilder->CreateCall(fromIntFunc, {L}, "int_to_str_tmp");
                llvm::Function *concatFunc = llvmModule->getFunction("Mycelium_String_concat");
                if (!concatFunc)
                {
                    log_error("Mycelium_String_concat not found", node->location);
                    return ExpressionVisitResult(nullptr);
                }
                llvm::Value *result_str_ptr = llvmBuilder->CreateCall(concatFunc, {l_as_str, R}, "concat_int_str");
                return ExpressionVisitResult(result_str_ptr, nullptr);
            }
        }
        if (LType != RType)
        {
            if (LType->isFloatingPointTy() && RType->isIntegerTy())
            {
                R = llvmBuilder->CreateSIToFP(R, LType, "inttofp_tmp");
                RType = LType;
            }
            else if (RType->isFloatingPointTy() && LType->isIntegerTy())
            {
                L = llvmBuilder->CreateSIToFP(L, RType, "inttofp_tmp");
                LType = RType;
            }
            else
            {
                // Type compatibility already validated by semantic analyzer
                // Proceed with code generation
            }
        }
        llvm::Value *result_val = nullptr;
        switch (node->opKind)
        {
        case BinaryOperatorKind::Add:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateAdd(L, R, "addtmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFAdd(L, R, "faddtmp");
            else
                log_error("Unsupported type for Add: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::Subtract:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateSub(L, R, "subtmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFSub(L, R, "fsubtmp");
            else
                log_error("Unsupported type for Subtract: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::Multiply:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateMul(L, R, "multmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFMul(L, R, "fmultmp");
            else
                log_error("Unsupported type for Multiply: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::Divide:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateSDiv(L, R, "sdivtmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFDiv(L, R, "fdivtmp");
            else
                log_error("Unsupported type for Divide: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::Modulo:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateSRem(L, R, "sremtmp");
            else
                log_error("Unsupported type for Modulo: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::Equals:
            if (LType->isIntegerTy() || LType->isPointerTy())
                result_val = llvmBuilder->CreateICmpEQ(L, R, "eqtmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFCmpOEQ(L, R, "feqtmp");
            else
                log_error("Unsupported type for Equals: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::NotEquals:
            if (LType->isIntegerTy() || LType->isPointerTy())
                result_val = llvmBuilder->CreateICmpNE(L, R, "netmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFCmpONE(L, R, "fnetmp");
            else
                log_error("Unsupported type for NotEquals: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::LessThan:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateICmpSLT(L, R, "slttmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFCmpOLT(L, R, "folttmp");
            else
                log_error("Unsupported type for LessThan: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::GreaterThan:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateICmpSGT(L, R, "sgttmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFCmpOGT(L, R, "fogttmp");
            else
                log_error("Unsupported type for GreaterThan: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::LessThanOrEqual:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateICmpSLE(L, R, "sletmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFCmpOLE(L, R, "foletmp");
            else
                log_error("Unsupported type for LessThanOrEqual: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::GreaterThanOrEqual:
            if (LType->isIntegerTy())
                result_val = llvmBuilder->CreateICmpSGE(L, R, "sgetmp");
            else if (LType->isFloatingPointTy())
                result_val = llvmBuilder->CreateFCmpOGE(L, R, "fogetmp");
            else
                log_error("Unsupported type for GreaterThanOrEqual: " + llvm_type_to_string(LType), node->location);
            break;
        case BinaryOperatorKind::LogicalAnd:
            if (LType->isIntegerTy(1) && RType->isIntegerTy(1))
                result_val = llvmBuilder->CreateAnd(L, R, "andtmp");
            else
                log_error("LogicalAnd requires boolean operands.", node->location);
            break;
        case BinaryOperatorKind::LogicalOr:
            if (LType->isIntegerTy(1) && RType->isIntegerTy(1))
                result_val = llvmBuilder->CreateOr(L, R, "ortmp");
            else
                log_error("LogicalOr requires boolean operands.", node->location);
            break;
        default:
            log_error("Unsupported binary operator.", node->location);
            return ExpressionVisitResult(nullptr);
        }
        return ExpressionVisitResult(result_val, nullptr);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<AssignmentExpressionNode> node)
    {
        ExpressionVisitResult source_res = visit(node->source);
        llvm::Value *new_llvm_val = source_res.value;
        const ClassTypeInfo *new_val_static_ci = source_res.classInfo;
        if (!new_llvm_val)
        {
            log_error("Assignment source is null.", node->source->location);
            return ExpressionVisitResult(nullptr);
        }
        if (auto id_target = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target))
        {
            auto it = namedValues.find(id_target->identifier->name);
            if (it == namedValues.end())
            {
                // Try implicit field assignment: if we're in an instance method/constructor and target not found,
                // try to resolve it as this.fieldName assignment
                auto this_it = namedValues.find("this");
                if (this_it != namedValues.end() && this_it->second.classInfo)
                {
                    const ClassTypeInfo *class_info = this_it->second.classInfo;

                    // Check if the identifier matches a field name
                    auto field_it = class_info->field_indices.find(id_target->identifier->name);
                    if (field_it != class_info->field_indices.end())
                    {
                        // Create a member access assignment: this.fieldName = source
                        auto this_expr = std::make_shared<IdentifierExpressionNode>();
                        this_expr->identifier = std::make_shared<IdentifierNode>("this");
                        this_expr->location = id_target->location;

                        auto member_name = std::make_shared<IdentifierNode>(id_target->identifier->name);

                        auto member_access = std::make_shared<MemberAccessExpressionNode>();
                        member_access->target = this_expr;
                        member_access->memberName = member_name;
                        member_access->location = id_target->location;

                        // Create a new assignment with member access as target
                        auto member_assignment = std::make_shared<AssignmentExpressionNode>();
                        member_assignment->target = member_access;
                        member_assignment->source = node->source;
                        member_assignment->location = node->location;

                        // Recursively resolve the member access assignment
                        return visit(member_assignment);
                    }
                }

                log_error("Assigning to undeclared var: " + id_target->identifier->name, id_target->location);
                return ExpressionVisitResult(nullptr);
            }
            VariableInfo &target_var_info = it->second;
            llvm::Type *target_llvm_type = target_var_info.alloca->getAllocatedType();
            const ClassTypeInfo *target_static_ci = target_var_info.classInfo;

            // Type compatibility already validated by semantic analyzer
            // No need for redundant checks here

            // CRITICAL FIX: Only retain if source is NOT a new expression
            // New expressions already have correct ref_count, retaining them causes double-retain bug
            llvm::Value *new_object_header_for_retain = nullptr;
            if (new_val_static_ci && new_val_static_ci->fieldsType)
            {
                // Check if the source is a new expression (ObjectCreationExpression)
                bool is_new_expression = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node->source) != nullptr;

                if (!is_new_expression)
                {
                    // Only retain if this is NOT a new expression
                    if (source_res.header_ptr)
                    {
                        new_object_header_for_retain = source_res.header_ptr;
                    }
                    else
                    {
                        new_object_header_for_retain = getHeaderPtrFromFieldsPtr(new_llvm_val, new_val_static_ci->fieldsType);
                    }
                    if (new_object_header_for_retain)
                    {
                        llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_retain"), {new_object_header_for_retain});
                    }
                }
            }

            llvm::Value *old_llvm_val = llvmBuilder->CreateLoad(target_llvm_type, target_var_info.alloca, "old.val.assign");
            if (target_static_ci && target_static_ci->destructor_func)
            {
                llvm::Value *is_null_cond = llvmBuilder->CreateICmpNE(old_llvm_val, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(old_llvm_val->getType())));
                llvm::BasicBlock *dtor_call_bb = llvm::BasicBlock::Create(*llvmContext, "dtor.call.assign", currentFunction);
                llvm::BasicBlock *after_dtor_bb = llvm::BasicBlock::Create(*llvmContext, "after.dtor.assign", currentFunction);
                llvmBuilder->CreateCondBr(is_null_cond, dtor_call_bb, after_dtor_bb);
                llvmBuilder->SetInsertPoint(dtor_call_bb);
                llvmBuilder->CreateCall(target_static_ci->destructor_func, {old_llvm_val});
                llvmBuilder->CreateBr(after_dtor_bb);
                llvmBuilder->SetInsertPoint(after_dtor_bb);
            }
            if (target_static_ci && target_static_ci->fieldsType)
            {
                llvm::Value *old_hdr = getHeaderPtrFromFieldsPtr(old_llvm_val, target_static_ci->fieldsType);
                if (old_hdr)
                {
                    llvm::Value *is_old_hdr_null_cond = llvmBuilder->CreateICmpNE(old_hdr, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(old_hdr->getType())));
                    llvm::BasicBlock *release_call_bb = llvm::BasicBlock::Create(*llvmContext, "release.call.assign", currentFunction);
                    llvm::BasicBlock *after_release_bb = llvm::BasicBlock::Create(*llvmContext, "after.release.assign", currentFunction);
                    llvmBuilder->CreateCondBr(is_old_hdr_null_cond, release_call_bb, after_release_bb);
                    llvmBuilder->SetInsertPoint(release_call_bb);
                    llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_release"), {old_hdr});
                    llvmBuilder->CreateBr(after_release_bb);
                    llvmBuilder->SetInsertPoint(after_release_bb);
                }
            }
            llvmBuilder->CreateStore(new_llvm_val, target_var_info.alloca);

            // CRITICAL FIX: For instance method field assignments, also write back to the actual object field
            if (currentFunction && namedValues.count("this"))
            {
                // Check if this variable corresponds to a field in the current class
                const VariableInfo &thisInfo = namedValues["this"];
                if (thisInfo.classInfo && thisInfo.classInfo->field_indices.count(id_target->identifier->name))
                {
                    // This is a field assignment in an instance method - write back to the actual field
                    llvm::Value *this_fields_ptr = llvmBuilder->CreateLoad(thisInfo.alloca->getAllocatedType(), thisInfo.alloca, "this.for.field.assign");
                    unsigned field_idx = thisInfo.classInfo->field_indices.at(id_target->identifier->name);
                    llvm::Value *actual_field_ptr = llvmBuilder->CreateStructGEP(thisInfo.classInfo->fieldsType, this_fields_ptr, field_idx, id_target->identifier->name + ".actual.field.ptr");
                    llvmBuilder->CreateStore(new_llvm_val, actual_field_ptr);
                }
            }

            // NOTE: ARC tracking is now handled exclusively by the scope manager
            // No need for manual tracking in current_function_arc_locals map
        }
        else if (auto member_target = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target))
        {
            ExpressionVisitResult obj_res = visit(member_target->target);
            if (!obj_res.value || !obj_res.classInfo || !obj_res.classInfo->fieldsType)
            {
                log_error("Invalid member assignment target.", member_target->target->location);
                return ExpressionVisitResult(nullptr);
            }
            auto field_it = obj_res.classInfo->field_indices.find(member_target->memberName->name);
            if (field_it == obj_res.classInfo->field_indices.end())
            {
                log_error("Field not found in assignment", member_target->location);
                return ExpressionVisitResult(nullptr);
            }
            unsigned field_idx = field_it->second;
            llvm::Value *field_ptr = llvmBuilder->CreateStructGEP(obj_res.classInfo->fieldsType, obj_res.value, field_idx);

            // ARC: Release old field value before storing new value
            llvm::Type *field_type = obj_res.classInfo->fieldsType->getElementType(field_idx);
            auto field_ast_type = obj_res.classInfo->field_ast_types[field_idx];

            // Check if this is an object field that needs ARC management
            const ClassTypeInfo *field_class_info = nullptr;
            if (field_type->isPointerTy() && field_ast_type)
            {
                if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&field_ast_type->name_segment))
                {
                    auto field_cti_it = classTypeRegistry.find((*identNode)->name);
                    if (field_cti_it != classTypeRegistry.end())
                    {
                        field_class_info = &field_cti_it->second;
                    }
                }
            }

            if (field_class_info && field_class_info->fieldsType)
            {
                // Load old field value and release it if not null
                llvm::Value *old_field_val = llvmBuilder->CreateLoad(field_type, field_ptr, "old.field.val");

                // First check if the old field value itself is not null
                llvm::Value *is_field_not_null = llvmBuilder->CreateICmpNE(old_field_val, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(old_field_val->getType())));
                llvm::BasicBlock *check_release_bb = llvm::BasicBlock::Create(*llvmContext, "check.release.field", currentFunction);
                llvm::BasicBlock *after_release_bb = llvm::BasicBlock::Create(*llvmContext, "after.release.field", currentFunction);
                llvmBuilder->CreateCondBr(is_field_not_null, check_release_bb, after_release_bb);

                llvmBuilder->SetInsertPoint(check_release_bb);
                llvm::Value *old_hdr = getHeaderPtrFromFieldsPtr(old_field_val, field_class_info->fieldsType);
                if (old_hdr)
                {
                    llvm::Value *is_hdr_not_null = llvmBuilder->CreateICmpNE(old_hdr, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(old_hdr->getType())));
                    llvm::BasicBlock *release_bb = llvm::BasicBlock::Create(*llvmContext, "release.old.field", currentFunction);
                    llvm::BasicBlock *skip_release_bb = llvm::BasicBlock::Create(*llvmContext, "skip.release.field", currentFunction);
                    llvmBuilder->CreateCondBr(is_hdr_not_null, release_bb, skip_release_bb);

                    llvmBuilder->SetInsertPoint(release_bb);
                    llvmBuilder->CreateCall(llvmModule->getFunction("Mycelium_Object_release"), {old_hdr});
                    llvmBuilder->CreateBr(skip_release_bb);

                    llvmBuilder->SetInsertPoint(skip_release_bb);
                    llvmBuilder->CreateBr(after_release_bb);
                }
                else
                {
                    llvmBuilder->CreateBr(after_release_bb);
                }
                llvmBuilder->SetInsertPoint(after_release_bb);
            }

            llvmBuilder->CreateStore(new_llvm_val, field_ptr);
        }
        else
        {
            log_error("Invalid assignment target.", node->target->location);
            return ExpressionVisitResult(nullptr);
        }
        return ExpressionVisitResult(new_llvm_val, new_val_static_ci);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<UnaryExpressionNode> node)
    {
        ExpressionVisitResult operand_res = visit(node->operand);
        if (!operand_res.value)
        {
            log_error("Operand for unary expression is null.", node->operand->location);
            return ExpressionVisitResult(nullptr);
        }
        llvm::Value *operand_val = operand_res.value;
        llvm::Value *result_val = nullptr;
        switch (node->opKind)
        {
        case UnaryOperatorKind::LogicalNot:
            result_val = llvmBuilder->CreateNot(operand_val, "nottmp");
            break;
        case UnaryOperatorKind::UnaryMinus:
            if (operand_val->getType()->isIntegerTy())
                result_val = llvmBuilder->CreateNeg(operand_val, "negtmp");
            else if (operand_val->getType()->isFloatingPointTy())
                result_val = llvmBuilder->CreateFNeg(operand_val, "fnegtmp");
            else
                log_error("Unsupported type for unary minus.", node->location);
            break;
        // TODO: Pre/Post Increment/Decrement need LValue handling
        // For now, they might not work correctly or might be unhandled.
        case UnaryOperatorKind::PreIncrement:  // Fallthrough
        case UnaryOperatorKind::PostIncrement: // Fallthrough
        case UnaryOperatorKind::PreDecrement:  // Fallthrough
        case UnaryOperatorKind::PostDecrement: // Fallthrough
            log_error("Pre/Post Increment/Decrement not fully implemented.", node->location);
            result_val = operand_val; // Placeholder: return operand itself
            break;
        default:
            log_error("Unsupported unary operator.", node->location);
            break;
        }
        return ExpressionVisitResult(result_val, nullptr);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<MethodCallExpressionNode> node)
    {
        std::string method_name;
        const ClassTypeInfo* callee_class_info = nullptr;
        llvm::Value* instance_ptr_for_call = nullptr;
        bool is_primitive_call = false;
        PrimitiveStructInfo* primitive_info = nullptr;

        // The target of a method call is typically a MemberAccessExpression or an IdentifierExpression.
        // We visit it to get the context for the call.
        if (auto member_access = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target))
        {
            method_name = member_access->memberName->name;
            // Visit the expression to the left of the dot (e.g., `myInstance` or `MyClass`)
            ExpressionVisitResult target_res = visit(member_access->target);

            callee_class_info = target_res.classInfo;
            instance_ptr_for_call = target_res.value; // Will be null for static calls.
            primitive_info = target_res.primitive_info;
            is_primitive_call = (primitive_info != nullptr);
        }
        else if (auto identifier = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target))
        {
            method_name = identifier->identifier->name;

            // This could be an extern function or an implicit call (static or instance).
            if (symbolTable && symbolTable->find_method(method_name) && symbolTable->find_method(method_name)->is_external)
            {
                // Extern function call. It has no class context.
            }
            else if (currentFunction)
            {
                // It's an implicit call within a method. We need to determine if it's static or instance.
                std::string current_func_name = currentFunction->getName().str();
                size_t dot_pos = current_func_name.find('.');
                if (dot_pos == std::string::npos) {
                    // Semantic analyzer should have caught this
                    return ExpressionVisitResult(nullptr);
                }
                std::string current_class_name = current_func_name.substr(0, dot_pos);

                // Find the target method symbol within the current class to check its properties.
                const auto* target_method_symbol = symbolTable->find_method_in_class(current_class_name, method_name);
                if (!target_method_symbol) {
                    // Semantic analyzer should have validated method exists
                    return ExpressionVisitResult(nullptr);
                }
                
                // Get the ClassTypeInfo for the current class.
                auto cti_it = classTypeRegistry.find(current_class_name);
                if (cti_it == classTypeRegistry.end()) {
                    // Class should exist if semantic analyzer passed
                    return ExpressionVisitResult(nullptr);
                }
                callee_class_info = &cti_it->second;

                // Check if the call is static or instance based on the target method's properties.
                if (target_method_symbol->is_static)
                {
                    // It's a static call. No instance pointer is needed.
                    instance_ptr_for_call = nullptr;
                }
                else // It's an instance call.
                {
                    // We need 'this'.
                    auto this_it = namedValues.find("this");
                    if (this_it == namedValues.end()) {
                        // Semantic analyzer should have caught static context errors
                        return ExpressionVisitResult(nullptr);
                    }
                    instance_ptr_for_call = llvmBuilder->CreateLoad(this_it->second.alloca->getAllocatedType(), this_it->second.alloca, "this.for.implicit.call");
                }
            }
            else
            {
                // Semantic analyzer should have caught global context errors
                return ExpressionVisitResult(nullptr);
            }
        }
        else
        {
            // Semantic analyzer should have validated method call targets
            return ExpressionVisitResult(nullptr);
        }

        // Now, build the function name and find it.
        std::string resolved_func_name;
        if (callee_class_info) {
            // Use semantic analyzer to find method in inheritance chain
            if (symbolTable)
            {
                const auto* method_symbol = symbolTable->find_method_in_class(callee_class_info->name, method_name);
                if (method_symbol)
                {
                    resolved_func_name = method_symbol->qualified_name;
                    LOG_DEBUG("Found method via inheritance: " + method_name + " -> " + resolved_func_name, "COMPILER");
                }
                else
                {
                    // Fallback to old behavior
                    resolved_func_name = callee_class_info->name + "." + method_name;
                    LOG_DEBUG("Method not found in inheritance chain, using direct name: " + resolved_func_name, "COMPILER");
                }
            }
            else
            {
                resolved_func_name = callee_class_info->name + "." + method_name;
            }
        } else if (!is_primitive_call) {
            resolved_func_name = method_name; // For extern functions
        }
        
        if (is_primitive_call && primitive_info)
        {
            return handle_primitive_method_call(node, primitive_info, instance_ptr_for_call);
        }

        // Check if this is a virtual method call that needs VTable dispatch
        bool use_virtual_dispatch = false;
        size_t virtual_method_index = 0;
        
        if (callee_class_info && instance_ptr_for_call && symbolTable)
        {
            const auto* method_symbol = symbolTable->find_method(resolved_func_name);
            if (method_symbol && method_symbol->is_virtual)
            {
                // Find the virtual method index in the class's VTable order
                const auto* class_symbol = symbolTable->find_class(callee_class_info->name);
                if (class_symbol)
                {
                    // Search for the method by name in the VTable, checking for inherited methods
                    for (size_t i = 0; i < class_symbol->virtual_method_order.size(); ++i)
                    {
                        // Extract method name from qualified name for comparison
                        const std::string& vtable_method = class_symbol->virtual_method_order[i];
                        size_t dot_pos = vtable_method.find_last_of('.');
                        if (dot_pos != std::string::npos)
                        {
                            std::string vtable_method_name = vtable_method.substr(dot_pos + 1);
                            if (vtable_method_name == method_name)
                            {
                                use_virtual_dispatch = true;
                                virtual_method_index = i + 1; // +1 for destructor slot at index 0
                                LOG_DEBUG("Found virtual method at VTable index " + std::to_string(i) + ": " + vtable_method, "COMPILER");
                                break;
                            }
                        }
                    }
                }
            }
        }

        std::vector<llvm::Value *> args_values;
        llvm::Value *call_result_val = nullptr;

        if (use_virtual_dispatch)
        {
            // Virtual method call via VTable lookup
            LOG_DEBUG("Using virtual dispatch for method: " + resolved_func_name, "COMPILER");
            
            // Get object header pointer from instance pointer (fields pointer)
            llvm::Value* header_ptr = getHeaderPtrFromFieldsPtr(instance_ptr_for_call, callee_class_info->fieldsType);
            
            // Load VTable pointer from object header (offset 8 for vtable field)
            llvm::Value* vtable_ptr_ptr = llvmBuilder->CreateConstInBoundsGEP1_64(
                llvm::Type::getInt8Ty(*llvmContext), header_ptr, 8, "vtable_ptr_ptr");
            llvm::Value* vtable_ptr = llvmBuilder->CreateLoad(
                llvm::PointerType::getUnqual(*llvmContext), vtable_ptr_ptr, "vtable_ptr");
            
            // Get function pointer from VTable at the correct index
            llvm::Value* method_ptr_ptr = llvmBuilder->CreateConstInBoundsGEP1_64(
                llvm::PointerType::getUnqual(*llvmContext), vtable_ptr, virtual_method_index, "method_ptr_ptr");
            llvm::Value* method_ptr = llvmBuilder->CreateLoad(
                llvm::PointerType::getUnqual(*llvmContext), method_ptr_ptr, "method_ptr");
            
            // Prepare arguments for virtual call (header pointer + method args)
            args_values.push_back(instance_ptr_for_call);
            
            if (node->argumentList)
            {
                for (const auto &arg_node : node->argumentList->arguments)
                {
                    ExpressionVisitResult arg_res = visit(arg_node->expression);
                    if (!arg_res.value)
                    {
                        // Semantic analyzer should have validated arguments
                        return ExpressionVisitResult(nullptr);
                    }
                    args_values.push_back(arg_res.value);
                }
            }
            
            // Create indirect call through function pointer
            llvm::Function *direct_callee = llvmModule->getFunction(resolved_func_name);
            if (!direct_callee)
            {
                // Semantic analyzer should have validated function exists
                return ExpressionVisitResult(nullptr);
            }
            
            llvm::FunctionType* func_type = direct_callee->getFunctionType();
            call_result_val = llvmBuilder->CreateCall(func_type, method_ptr, args_values, 
                func_type->getReturnType()->isVoidTy() ? "" : "virtual_call");
        }
        else
        {
            // Direct method call (non-virtual or static)
            llvm::Function *callee = llvmModule->getFunction(resolved_func_name);
            if (!callee)
            {
                // Semantic analyzer should have validated function exists
                return ExpressionVisitResult(nullptr);
            }

            if (instance_ptr_for_call)
            {
                args_values.push_back(instance_ptr_for_call);
            }
        
            if (node->argumentList)
            {
                for (const auto &arg_node : node->argumentList->arguments)
                {
                    ExpressionVisitResult arg_res = visit(arg_node->expression);
                    if (!arg_res.value)
                    {
                        // Semantic analyzer should have validated arguments
                        return ExpressionVisitResult(nullptr);
                    }
                    args_values.push_back(arg_res.value);
                }
            }

            // Verify argument count
            if (callee->arg_size() != args_values.size()) {
                log_error("Incorrect number of arguments for function " + resolved_func_name + ". Expected " + std::to_string(callee->arg_size()) + ", got " + std::to_string(args_values.size()), node->location);
                return ExpressionVisitResult(nullptr);
            }

            call_result_val = llvmBuilder->CreateCall(callee, args_values, callee->getReturnType()->isVoidTy() ? "" : "calltmp");
        }

        const ClassTypeInfo *return_static_ci = nullptr;
        if (!use_virtual_dispatch)
        {
            // For direct calls, we can look up return type info
            llvm::Function *callee = llvmModule->getFunction(resolved_func_name);
            if (callee)
            {
                auto return_info_it = functionReturnClassInfoMap.find(callee);
                if (return_info_it != functionReturnClassInfoMap.end())
                {
                    return_static_ci = return_info_it->second;
                }
            }
        }
        // For virtual calls, return type info handling can be enhanced later

        return ExpressionVisitResult(call_result_val, return_static_ci);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<ObjectCreationExpressionNode> node)
    {
        if (!node->type)
        {
            log_error("Object creation missing type.", node->location);
            return ExpressionVisitResult(nullptr);
        }
        std::string class_name_str;
        if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment))
            class_name_str = (*identNode)->name;
        else
        {
            log_error("Unsupported type in new.", node->type->location);
            return ExpressionVisitResult(nullptr);
        }
        auto cti_it = classTypeRegistry.find(class_name_str);
        if (cti_it == classTypeRegistry.end())
        {
            log_error("Undefined class in new: " + class_name_str, node->type->location);
            return ExpressionVisitResult(nullptr);
        }
        const ClassTypeInfo &cti = cti_it->second;
        if (!cti.fieldsType)
        {
            log_error("Class " + class_name_str + " has no fieldsType.", node->type->location);
            return ExpressionVisitResult(nullptr);
        }
        llvm::DataLayout dl = llvmModule->getDataLayout();
        llvm::Value *data_size_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvmContext), dl.getTypeAllocSize(cti.fieldsType));
        llvm::Value *type_id_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvmContext), cti.type_id);
        llvm::Function *alloc_func = llvmModule->getFunction("Mycelium_Object_alloc");
        if (!alloc_func)
        {
            log_error("Runtime Mycelium_Object_alloc not found.", node->location);
            return ExpressionVisitResult(nullptr);
        }
        // Pass the actual VTable for the class (Sweep 2.5 polymorphism support)
        llvm::Value *vtable_ptr_val;
        if (cti.vtable_global)
        {
            // Cast VTable global to generic pointer for runtime use
            vtable_ptr_val = llvmBuilder->CreateBitCast(cti.vtable_global, llvm::PointerType::getUnqual(*llvmContext), "vtable_ptr");
        }
        else
        {
            // No VTable for this class (no virtual methods)
            vtable_ptr_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*llvmContext));
        }
        llvm::Value *header_ptr_val = llvmBuilder->CreateCall(alloc_func, {data_size_val, type_id_val, vtable_ptr_val}, "new.header");
        llvm::Value *fields_obj_opaque_ptr = getFieldsPtrFromHeaderPtr(header_ptr_val, cti.fieldsType);
        std::string ctor_name_str = class_name_str + ".%ctor";
        std::vector<llvm::Value *> ctor_args_values = {fields_obj_opaque_ptr};
        if (node->argumentList.has_value())
        {
            for (const auto &arg_node : node->argumentList.value()->arguments)
            {
                ctor_args_values.push_back(visit(arg_node->expression).value);
            }
        }
        llvm::Function *constructor_func = llvmModule->getFunction(ctor_name_str);
        if (!constructor_func)
        {
            log_error("Constructor " + ctor_name_str + " not found.", node->location);
            return ExpressionVisitResult(nullptr);
        }
        llvmBuilder->CreateCall(constructor_func, ctor_args_values);
        return ExpressionVisitResult(fields_obj_opaque_ptr, &cti, header_ptr_val);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<ThisExpressionNode> node)
    {
        auto it = namedValues.find("this");
        if (it == namedValues.end())
        {
            log_error("'this' used inappropriately.", node->location);
            return ExpressionVisitResult(nullptr);
        }
        const VariableInfo &thisVarInfo = it->second;
        llvm::Value *loaded_this_ptr = llvmBuilder->CreateLoad(thisVarInfo.alloca->getAllocatedType(), thisVarInfo.alloca, "this.val");
        return ExpressionVisitResult(loaded_this_ptr, thisVarInfo.classInfo);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<CastExpressionNode> node)
    {
        ExpressionVisitResult expr_to_cast_res = visit(node->expression);
        llvm::Value *expr_val = expr_to_cast_res.value;
        if (!expr_val)
        {
            log_error("Expression to be cast is null.", node->expression->location);
            return ExpressionVisitResult(nullptr);
        }
        llvm::Type *target_llvm_type = get_llvm_type(node->targetType);
        if (!target_llvm_type)
        {
            log_error("Target type for cast is null.", node->targetType->location);
            return ExpressionVisitResult(nullptr);
        }
        const ClassTypeInfo *target_static_ci = nullptr;
        if (target_llvm_type->isPointerTy())
        {
            if (auto typeNameNode = node->targetType)
            {
                if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&typeNameNode->name_segment))
                {
                    auto cti_it = classTypeRegistry.find((*identNode)->name);
                    if (cti_it != classTypeRegistry.end())
                    {
                        target_static_ci = &cti_it->second;
                    }
                }
            }
        }
        llvm::Value *cast_val = nullptr;
        llvm::Type *src_llvm_type = expr_val->getType();
        
        // Special case: Primitive-to-string conversions using ToString() methods
        if (target_llvm_type->isPointerTy())
        {
            if (auto typeNameNode = node->targetType)
            {
                if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&typeNameNode->name_segment))
                {
                    std::string target_type_name = (*identNode)->name;
                    if (target_type_name == "string")
                    {
                        // Check if we're converting from a primitive type to string
                        if (src_llvm_type->isIntegerTy())
                        {
                            unsigned bit_width = src_llvm_type->getIntegerBitWidth();
                            if (bit_width == 32)
                            {
                                // int to string
                                llvm::Function *int_to_string_func = llvmModule->getFunction("Mycelium_String_from_int");
                                if (int_to_string_func)
                                {
                                    cast_val = llvmBuilder->CreateCall(int_to_string_func, {expr_val}, "int_to_string");
                                }
                            }
                            else if (bit_width == 64)
                            {
                                // long to string
                                llvm::Function *long_to_string_func = llvmModule->getFunction("Mycelium_String_from_long");
                                if (long_to_string_func)
                                {
                                    cast_val = llvmBuilder->CreateCall(long_to_string_func, {expr_val}, "long_to_string");
                                }
                            }
                            else if (bit_width == 1)
                            {
                                // bool to string
                                llvm::Function *bool_to_string_func = llvmModule->getFunction("Mycelium_String_from_bool");
                                if (bool_to_string_func)
                                {
                                    cast_val = llvmBuilder->CreateCall(bool_to_string_func, {expr_val}, "bool_to_string");
                                }
                            }
                            else if (bit_width == 8)
                            {
                                // char to string
                                llvm::Function *char_to_string_func = llvmModule->getFunction("Mycelium_String_from_char");
                                if (char_to_string_func)
                                {
                                    cast_val = llvmBuilder->CreateCall(char_to_string_func, {expr_val}, "char_to_string");
                                }
                            }
                        }
                        else if (src_llvm_type->isFloatTy())
                        {
                            // float to string
                            llvm::Function *float_to_string_func = llvmModule->getFunction("Mycelium_String_from_float");
                            if (float_to_string_func)
                            {
                                cast_val = llvmBuilder->CreateCall(float_to_string_func, {expr_val}, "float_to_string");
                            }
                        }
                        else if (src_llvm_type->isDoubleTy())
                        {
                            // double to string
                            llvm::Function *double_to_string_func = llvmModule->getFunction("Mycelium_String_from_double");
                            if (double_to_string_func)
                            {
                                cast_val = llvmBuilder->CreateCall(double_to_string_func, {expr_val}, "double_to_string");
                            }
                        }
                        
                        if (cast_val)
                        {
                            return ExpressionVisitResult(cast_val, target_static_ci);
                        }
                    }
                }
            }
        }
        
        if (target_llvm_type == src_llvm_type)
        {
            cast_val = expr_val;
        }
        else if (target_llvm_type->isIntegerTy() && src_llvm_type->isFloatingPointTy())
        {
            cast_val = llvmBuilder->CreateFPToSI(expr_val, target_llvm_type, "fptosi_cast");
        }
        else if (target_llvm_type->isFloatingPointTy() && src_llvm_type->isIntegerTy())
        {
            cast_val = llvmBuilder->CreateSIToFP(expr_val, target_llvm_type, "sitofp_cast");
        }
        else if (target_llvm_type->isIntegerTy() && src_llvm_type->isIntegerTy())
        {
            unsigned target_width = target_llvm_type->getIntegerBitWidth();
            unsigned src_width = src_llvm_type->getIntegerBitWidth();
            if (target_width > src_width)
            {
                cast_val = llvmBuilder->CreateSExt(expr_val, target_llvm_type, "sext_cast");
            }
            else if (target_width < src_width)
            {
                cast_val = llvmBuilder->CreateTrunc(expr_val, target_llvm_type, "trunc_cast");
            }
            else
            {
                cast_val = expr_val;
            }
        }
        else if (target_llvm_type->isPointerTy() && src_llvm_type->isPointerTy())
        {
            cast_val = llvmBuilder->CreateBitCast(expr_val, target_llvm_type, "ptr_bitcast");
        }
        else if (target_llvm_type->isIntegerTy() && src_llvm_type->isPointerTy())
        {
            cast_val = llvmBuilder->CreatePtrToInt(expr_val, target_llvm_type, "ptrtoint_cast");
        }
        else if (target_llvm_type->isPointerTy() && src_llvm_type->isIntegerTy())
        {
            cast_val = llvmBuilder->CreateIntToPtr(expr_val, target_llvm_type, "inttoptr_cast");
        }
        else
        {
            log_error("Unsupported cast from " + llvm_type_to_string(src_llvm_type) + " to " + llvm_type_to_string(target_llvm_type), node->location);
            return ExpressionVisitResult(nullptr, nullptr);
        }
        return ExpressionVisitResult(cast_val, target_static_ci);
    }

        
    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<MemberAccessExpressionNode> node)
    {
        ExpressionVisitResult target_res = visit(node->target);
        std::string member_name = node->memberName->name;

        // Case 1: Target is a namespace (e.g., MyCompany.Services)
        if (!target_res.resolved_path.empty() && target_res.classInfo == nullptr)
        {
            std::string new_path = target_res.resolved_path + "." + member_name;

            // Check if the new path resolves to a class
            if (symbolTable)
            {
                const auto* class_symbol = symbolTable->find_class(new_path);
                if (class_symbol)
                {
                    auto cti_it = classTypeRegistry.find(new_path);
                    if (cti_it != classTypeRegistry.end())
                    {
                        ExpressionVisitResult res;
                        res.classInfo = &cti_it->second;
                        res.is_static_type = true;
                        res.resolved_path = new_path;
                        return res;
                    }
                }
            }

            // Check if the new path is still a namespace prefix
            if (symbolTable)
            {
                const auto& all_classes = symbolTable->get_classes();
                for (const auto& [class_name, class_symbol] : all_classes)
                {
                    if (class_name.rfind(new_path + ".", 0) == 0)
                    {
                        ExpressionVisitResult res;
                        res.resolved_path = new_path;
                        return res;
                    }
                }
            }
            
            log_error("Symbol '" + member_name + "' not found in namespace '" + target_res.resolved_path + "'.", node->memberName->location);
            return ExpressionVisitResult(nullptr);
        }

        // Case 2: Target is a static class type or an instance
        if (target_res.classInfo)
        {
            // Check for a field first - inherited fields now accessible by original name
            std::string actual_field_name = member_name;
            const ClassTypeInfo* field_owner_class = target_res.classInfo;
            auto field_it = target_res.classInfo->field_indices.find(member_name);
            
            if (field_it != target_res.classInfo->field_indices.end())
            {
                LOG_DEBUG("Found field: " + member_name + " at index " + std::to_string(field_it->second), "COMPILER");
            }
            
            if (field_it != target_res.classInfo->field_indices.end())
            {
                if (target_res.is_static_type)
                {
                    // TODO: Handle static fields when they are supported.
                    log_error("Static fields are not yet supported. Cannot access '" + member_name + "'.", node->location);
                    return ExpressionVisitResult(nullptr);
                }

                if (!target_res.value)
                {
                    log_error("Cannot access field '" + member_name + "' on a null instance.", node->target->location);
                    return ExpressionVisitResult(nullptr);
                }

                // It's an instance field access.
                unsigned field_idx = field_it->second;
                llvm::Type *field_llvm_type = target_res.classInfo->fieldsType->getElementType(field_idx);
                llvm::Value *field_ptr = llvmBuilder->CreateStructGEP(target_res.classInfo->fieldsType, target_res.value, field_idx, actual_field_name + ".ptr");
                llvm::Value *loaded_field = llvmBuilder->CreateLoad(field_llvm_type, field_ptr, member_name);
                
                const ClassTypeInfo *field_static_ci = nullptr;
                if (field_llvm_type->isPointerTy() && field_idx < target_res.classInfo->field_ast_types.size())
                {
                    std::shared_ptr<TypeNameNode> field_ast_type = target_res.classInfo->field_ast_types[field_idx];
                    if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&field_ast_type->name_segment))
                    {
                        auto cti_it = classTypeRegistry.find((*identNode)->name);
                        if (cti_it != classTypeRegistry.end())
                            field_static_ci = &cti_it->second;
                    }
                }
                return ExpressionVisitResult(loaded_field, field_static_ci);
            }

            // If not a field, it might be a method. The MethodCall visitor will verify.
            // We just pass the target info up the chain.
            return target_res;
        }
        
        // Handle primitive member access (e.g., string.Length)
        if (target_res.primitive_info)
        {
            // This is a property/method on a primitive. Let the method call visitor handle it.
            return target_res;
        }
        
        log_error("Invalid target for member access. Not a class, instance, or namespace.", node->target->location);
        return ExpressionVisitResult(nullptr);
    }

    ScriptCompiler::ExpressionVisitResult ScriptCompiler::visit(std::shared_ptr<ParenthesizedExpressionNode> node)
    {
        if (!node || !node->expression)
        {
            log_error("ParenthesizedExpressionNode or its inner expression is null.", node ? node->location : std::nullopt);
            return ExpressionVisitResult(nullptr);
        }
        return visit(node->expression);
    }

} // namespace Mycelium::Scripting::Lang
