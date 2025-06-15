#include "sharpie/compiler/codegen/codegen.hpp"
#include "llvm/IR/DerivedTypes.h"
#include <functional>
#include <common/logger.hpp>
#include <typeinfo>



namespace Mycelium::Scripting::Lang::CodeGen {

CodeGenerator::CodeGenerator(CodeGenContext& context) : ctx(context) {
}

void CodeGenerator::generate(std::shared_ptr<CompilationUnitNode> ast_root) {
    if (!ast_root) {
        log_compiler_error("Cannot generate code from a null AST root.");
    }
    cg_compilation_unit(ast_root);
}

void CodeGenerator::cg_compilation_unit(std::shared_ptr<CompilationUnitNode> node) {
    // Process external declarations first
    for (const auto& ext_decl : node->externs) {
        cg_external_method_declaration(ext_decl);
    }

    // Helper lambda to recursively traverse namespaces and collect class declarations
    std::function<void(const std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>>&, const std::string&)> collect_classes_recursive;
    std::vector<std::pair<std::shared_ptr<ClassDeclarationNode>, std::string>> all_classes_with_context;

    collect_classes_recursive = 
        [&](const std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>>& members, const std::string& current_namespace) {
        for (const auto& member : members) {
            if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
                all_classes_with_context.push_back({class_decl, current_namespace});
            } else if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member)) {
                std::string next_namespace = current_namespace.empty() 
                                                ? ns_decl->name->name 
                                                : current_namespace + "." + ns_decl->name->name;
                collect_classes_recursive(ns_decl->members, next_namespace);
            }
        }
    };

    // Start collection from top level
    collect_classes_recursive(node->members, "");

    // PASS 1: Create class structures and declare ALL method signatures
    for (const auto& pair : all_classes_with_context) {
        std::shared_ptr<ClassDeclarationNode> class_decl = pair.first;
        const std::string& namespace_context = pair.second;
        std::string fq_class_name = namespace_context.empty() 
                                    ? class_decl->name->name 
                                    : namespace_context + "." + class_decl->name->name;
        cg_declare_class_structure_and_signatures(class_decl, fq_class_name);
    }

    // PASS 2: Compile ALL method bodies
    for (const auto& pair : all_classes_with_context) {
        std::shared_ptr<ClassDeclarationNode> class_decl = pair.first;
        const std::string& namespace_context = pair.second;
        std::string fq_class_name = namespace_context.empty() 
                                    ? class_decl->name->name 
                                    : namespace_context + "." + class_decl->name->name;
        cg_compile_all_method_bodies(class_decl, fq_class_name);
    }
    
    // PASS 3: Populate VTables
    for (const auto& pair : all_classes_with_context) {
        std::shared_ptr<ClassDeclarationNode> class_decl = pair.first;
        const std::string& namespace_context = pair.second;
        std::string fq_class_name = namespace_context.empty() 
                                    ? class_decl->name->name 
                                    : namespace_context + "." + class_decl->name->name;
        cg_populate_vtable_for_class(fq_class_name);
    }
}

void CodeGenerator::cg_external_method_declaration(std::shared_ptr<ExternalMethodDeclarationNode> node) {
    // Check if function already exists (e.g., from runtime bindings)
    if (ctx.llvm_module.getFunction(node->name->name)) {
        return; // Skip to avoid duplication
    }

    if (!node->type.has_value()) {
        log_compiler_error("External method lacks return type.", node->location);
    }

    llvm::Type* return_type = get_llvm_type(ctx, node->type.value());
    std::vector<llvm::Type*> param_types;
    for (const auto& param_node : node->parameters) {
        if (!param_node->type) {
            log_compiler_error("External method parameter lacks type.", param_node->location);
        }
        param_types.push_back(get_llvm_type(ctx, param_node->type));
    }

    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, node->name->name, &ctx.llvm_module);
}

void CodeGenerator::cg_declare_class_structure_and_signatures(std::shared_ptr<ClassDeclarationNode> node, const std::string& fq_class_name) {
    auto* class_symbol = ctx.symbol_table.find_class(fq_class_name);
    if (!class_symbol) {
        log_compiler_error("Class not found in symbol table: " + fq_class_name, node->location);
    }

    // CRITICAL FIX: Read field count from the correct location
    // Fields are stored in class_symbol->field_names, NOT class_symbol->field_names_in_order
    LOG_DEBUG("Setting up class: " + fq_class_name + " with " + 
              std::to_string(class_symbol->field_names.size()) + " fields", "COMPILER");
    for (size_t i = 0; i < class_symbol->field_names.size(); ++i) {
        LOG_DEBUG("  Field[" + std::to_string(i) + "]: " + class_symbol->field_names[i], "COMPILER");
    }

    // Build field types (inherited fields are already included in field_names/field_types by semantic analyzer)
    std::vector<llvm::Type*> field_llvm_types;
    std::vector<std::shared_ptr<TypeNameNode>> field_ast_types;
    
    // Populate the field_indices map for code generation
    class_symbol->field_indices.clear(); // Clear any existing indices
    unsigned field_index = 0;
    
    LOG_DEBUG("Processing fields for class: " + fq_class_name, "COMPILER");
    
    for (const auto& field_name : class_symbol->field_names) {
        LOG_DEBUG("  Processing field: " + field_name + " at index " + std::to_string(field_index), "COMPILER");
        
        const SymbolTable::VariableSymbol* field_sym = ctx.symbol_table.find_field_in_class(fq_class_name, field_name);
        if (!field_sym) {
            LOG_DEBUG("  WARNING: Field symbol not found for: " + field_name + " in class: " + fq_class_name, "COMPILER");
            
            // Try to get field type from the class symbol directly
            auto field_iter = std::find(class_symbol->field_names.begin(), class_symbol->field_names.end(), field_name);
            if (field_iter != class_symbol->field_names.end()) {
                size_t field_idx = std::distance(class_symbol->field_names.begin(), field_iter);
                if (field_idx < class_symbol->field_types.size()) {
                    std::shared_ptr<TypeNameNode> field_type = class_symbol->field_types[field_idx];
                    if (field_type) {
                        LOG_DEBUG("  Found field type in class symbol for: " + field_name, "COMPILER");
                        try {
                            llvm::Type* field_llvm_type = get_llvm_type(ctx, field_type);
                            if (field_llvm_type) {
                                field_llvm_types.push_back(field_llvm_type);
                                field_ast_types.push_back(field_type);
                                class_symbol->field_indices[field_name] = field_index;
                                LOG_DEBUG("  Mapped field '" + field_name + "' to index " + std::to_string(field_index), "COMPILER");
                                field_index++;
                            }
                        } catch (const std::exception& e) {
                            LOG_DEBUG("  ERROR processing field '" + field_name + "': " + e.what(), "COMPILER");
                        }
                    }
                }
            }
            continue;
        }
        
        if (!field_sym->type) {
            LOG_DEBUG("  WARNING: Field type is null for: " + field_name + " in class: " + fq_class_name, "COMPILER");
            continue;
        }
        
        try {
            llvm::Type* field_llvm_type = get_llvm_type(ctx, field_sym->type);
            if (!field_llvm_type) {
                LOG_DEBUG("  WARNING: Could not get LLVM type for field: " + field_name, "COMPILER");
                continue;
            }
            
            field_llvm_types.push_back(field_llvm_type);
            field_ast_types.push_back(field_sym->type);
            
            // CRITICAL: Map the field name to its index
            class_symbol->field_indices[field_name] = field_index;
            LOG_DEBUG("  Mapped field '" + field_name + "' to index " + std::to_string(field_index), "COMPILER");
            field_index++;
        } catch (const std::exception& e) {
            LOG_DEBUG("  ERROR processing field '" + field_name + "': " + e.what(), "COMPILER");
            throw;
        }
    }
    
    // Only create the struct type if it doesn't exist
    if (!class_symbol->fieldsType) {
        if (field_llvm_types.empty()) {
            LOG_DEBUG("Creating empty struct for class: " + fq_class_name, "COMPILER");
            class_symbol->fieldsType = llvm::StructType::create(ctx.llvm_context, {}, fq_class_name + "_Fields");
        } else {
            LOG_DEBUG("Creating struct with " + std::to_string(field_llvm_types.size()) + " fields for class: " + fq_class_name, "COMPILER");
            class_symbol->fieldsType = llvm::StructType::create(ctx.llvm_context, field_llvm_types, fq_class_name + "_Fields");
        }
        class_symbol->field_ast_types = field_ast_types;
    }

    // Generate VTable if class has virtual methods
    if (class_symbol && !class_symbol->virtual_method_order.empty() && !class_symbol->vtable_type) {
        LOG_DEBUG("Generating VTable for class: " + fq_class_name, "COMPILER");
        cg_generate_vtable_for_class(*class_symbol, class_symbol, fq_class_name);
    }
    
    // DEBUG: Log final field mapping
    LOG_DEBUG("Final field indices for " + fq_class_name + ":", "COMPILER");
    for (const auto& [field_name, index] : class_symbol->field_indices) {
        LOG_DEBUG("  " + field_name + " -> " + std::to_string(index), "COMPILER");
    }

    // Declare all method signatures for this class (these functions will check for duplicates)
    for (const auto& member : node->members) {
        if (auto methodDecl = std::dynamic_pointer_cast<MethodDeclarationNode>(member)) {
            cg_declare_method_signature(methodDecl, fq_class_name);
        } else if (auto ctorDecl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member)) {
            cg_declare_constructor_signature(ctorDecl, fq_class_name);
        } else if (auto dtorDecl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member)) {
            cg_declare_destructor_signature(dtorDecl, fq_class_name);
        }
    }
}

void CodeGenerator::cg_compile_all_method_bodies(std::shared_ptr<ClassDeclarationNode> node, const std::string& fq_class_name) {
    for (const auto& member : node->members) {
        if (auto methodDecl = std::dynamic_pointer_cast<MethodDeclarationNode>(member)) {
            cg_compile_method_body(methodDecl, fq_class_name);
        } else if (auto ctorDecl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member)) {
            cg_compile_constructor_body(ctorDecl, fq_class_name);
        } else if (auto dtorDecl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member)) {
            cg_compile_destructor_body(dtorDecl, fq_class_name);
        }
    }
}

llvm::Function* CodeGenerator::cg_declare_method_signature(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name) {
    bool is_static = false;
    for (const auto& mod : node->modifiers) {
        if (mod.first == ModifierKind::Static) {
            is_static = true;
            break;
        }
    }
    
    std::string func_name = class_name + "." + node->name->name;
    
    // Check if function already exists
    llvm::Function* existing_function = ctx.llvm_module.getFunction(func_name);
    if (existing_function) {
        return existing_function; // Already declared
    }
    
    if (!node->type.has_value()) {
        log_compiler_error("Method lacks return type.", node->location);
    }
    llvm::Type* return_type = get_llvm_type(ctx, node->type.value());

    std::vector<llvm::Type*> param_llvm_types;

    if (!is_static) {
        param_llvm_types.push_back(llvm::PointerType::get(ctx.llvm_context, 0));
    }
    
    for (const auto& param_node : node->parameters) {
        param_llvm_types.push_back(get_llvm_type(ctx, param_node->type));
    }

    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
    llvm::Function* function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, &ctx.llvm_module);

    // Map function to return class info if it returns an object
    if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type.value()->name_segment)) {
        if (auto identNode = *identNode_variant) {
            const auto* class_symbol = ctx.symbol_table.find_class(identNode->name);
            if (class_symbol) {
                ctx.function_return_class_info_map[function] = class_symbol;
            }
        }
    }

    return function;
}

void CodeGenerator::cg_compile_method_body(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name) {
    ctx.named_values.clear();
    
    bool is_static = false;
    for (const auto& mod : node->modifiers) {
        if (mod.first == ModifierKind::Static) {
            is_static = true;
            break;
        }
    }

    std::string func_name = class_name + "." + node->name->name;
    llvm::Function* function = ctx.llvm_module.getFunction(func_name);
    if (!function) {
        log_compiler_error("Function signature not found during body compilation: " + func_name, node->location);
    }

    ctx.scope_manager.push_scope(ScopeType::Function, func_name);
    ctx.current_function = function;

    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(ctx.llvm_context, "entry", function);
    ctx.builder.SetInsertPoint(entry_block);

    auto llvm_arg_it = function->arg_begin();
    const SymbolTable::ClassSymbol* this_class_info = nullptr;

    if (!is_static) {
        const auto* class_symbol = ctx.symbol_table.find_class(class_name);
        if (!class_symbol) {
            log_compiler_error("Class not found for instance method: " + class_name, node->location);
        }
        this_class_info = class_symbol;
        
        VariableInfo thisVarInfo;
        thisVarInfo.alloca = create_entry_block_alloca(ctx, "this", llvm_arg_it->getType());
        thisVarInfo.classInfo = this_class_info;
        ctx.builder.CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
        ctx.named_values["this"] = thisVarInfo;

        // Note: Field access in instance methods should use explicit this.field syntax
        // Simple field references like "value" should be parsed as "this.value"

        ++llvm_arg_it;
    }

    unsigned ast_param_idx = 0;
    for (; llvm_arg_it != function->arg_end(); ++llvm_arg_it, ++ast_param_idx) {
        VariableInfo paramVarInfo;
        const auto& param_node = node->parameters[ast_param_idx];
        paramVarInfo.alloca = create_entry_block_alloca(ctx, param_node->name->name, llvm_arg_it->getType());
        paramVarInfo.declaredTypeNode = param_node->type;

        if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&param_node->type->name_segment)) {
            if (auto identNode = *identNode_variant) {
                const auto* class_symbol = ctx.symbol_table.find_class(identNode->name);
                if (class_symbol) {
                    paramVarInfo.classInfo = class_symbol;
                }
            }
        }

        ctx.builder.CreateStore(&*llvm_arg_it, paramVarInfo.alloca);
        ctx.named_values[param_node->name->name] = paramVarInfo;
    }

    if (node->body) {
        cg_statement(node->body.value());
        if (!ctx.builder.GetInsertBlock()->getTerminator()) {
            ctx.scope_manager.pop_scope();
            if (function->getReturnType()->isVoidTy()) {
                ctx.builder.CreateRetVoid();
            } else {
                log_compiler_error("Non-void function '" + func_name + "' missing return.", node->body.value()->location);
            }
        }
        // Note: If there's already a terminator (return statement), the return visitor already popped the scope
    } else {
        log_compiler_error("Method '" + func_name + "' has no body.", node->location);
    }
    
    ctx.current_function = nullptr;
}

llvm::Function* CodeGenerator::cg_declare_constructor_signature(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name) {
    std::string func_name = class_name + ".%ctor";
    
    // Check if function already exists
    llvm::Function* existing_function = ctx.llvm_module.getFunction(func_name);
    if (existing_function) {
        return existing_function; // Already declared
    }
    
    llvm::Type* return_type = llvm::Type::getVoidTy(ctx.llvm_context);
    std::vector<llvm::Type*> param_llvm_types;
    param_llvm_types.push_back(llvm::PointerType::get(ctx.llvm_context, 0)); // 'this' pointer

    for (const auto& param_node : node->parameters) {
        if (!param_node->type) {
            log_compiler_error("Constructor parameter lacks type.", param_node->location);
        }
        param_llvm_types.push_back(get_llvm_type(ctx, param_node->type));
    }

    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
    return llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, &ctx.llvm_module);
}

void CodeGenerator::cg_compile_constructor_body(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name) {
    ctx.named_values.clear();
    std::string func_name = class_name + ".%ctor";
    llvm::Function* function = ctx.llvm_module.getFunction(func_name);
    if (!function) {
        log_compiler_error("Constructor signature not found: " + func_name, node->location);
    }
    
    const auto* class_symbol = ctx.symbol_table.find_class(class_name);
    if (!class_symbol) {
        log_compiler_error("Class not found for constructor: " + class_name, node->location);
    }
    const SymbolTable::ClassSymbol* this_class_info = class_symbol;

    ctx.scope_manager.push_scope(ScopeType::Function, func_name);
    ctx.current_function = function;

    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(ctx.llvm_context, "entry", function);
    ctx.builder.SetInsertPoint(entry_block);
    
    auto llvm_arg_it = function->arg_begin();
    VariableInfo thisVarInfo;
    thisVarInfo.alloca = create_entry_block_alloca(ctx, "this.ctor.arg", llvm_arg_it->getType());
    thisVarInfo.classInfo = this_class_info;
    ctx.builder.CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
    ctx.named_values["this"] = thisVarInfo;
    ++llvm_arg_it;
    
    // Process parameters
    unsigned ast_param_idx = 0;
    for (; llvm_arg_it != function->arg_end(); ++llvm_arg_it, ++ast_param_idx) {
        if (ast_param_idx >= node->parameters.size()) {
            log_compiler_error("LLVM argument count mismatch for constructor " + func_name, node->location);
        }
        const auto& ast_param = node->parameters[ast_param_idx];
        VariableInfo paramVarInfo;
        paramVarInfo.alloca = create_entry_block_alloca(ctx, ast_param->name->name, llvm_arg_it->getType());
        paramVarInfo.declaredTypeNode = ast_param->type;
        
        if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&ast_param->type->name_segment)) {
            if (auto identNode = *identNode_variant) {
                const auto* param_class_symbol = ctx.symbol_table.find_class(identNode->name);
                if (param_class_symbol) {
                    paramVarInfo.classInfo = param_class_symbol;
                }
            }
        }
        
        ctx.builder.CreateStore(&*llvm_arg_it, paramVarInfo.alloca);
        ctx.named_values[ast_param->name->name] = paramVarInfo;
    }
    
    if (node->body) {
        cg_statement(node->body.value());
        if (!ctx.builder.GetInsertBlock()->getTerminator()) {
            ctx.scope_manager.pop_scope();
            ctx.builder.CreateRetVoid();
        }
    } else {
        log_compiler_error("Constructor must have a body.", node->location);
    }
    
    ctx.current_function = nullptr;
}

llvm::Function* CodeGenerator::cg_declare_destructor_signature(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name) {
    std::string func_name = class_name + ".%dtor";
    
    // Check if function already exists
    llvm::Function* existing_function = ctx.llvm_module.getFunction(func_name);
    if (existing_function) {
        // Update the ClassSymbol with the existing function
        auto* class_symbol = ctx.symbol_table.find_class(class_name);
        if (class_symbol) {
            class_symbol->destructor_func = existing_function;
        }
        return existing_function; // Already declared
    }
    
    llvm::Type* return_type = llvm::Type::getVoidTy(ctx.llvm_context);
    std::vector<llvm::Type*> param_llvm_types = {llvm::PointerType::get(ctx.llvm_context, 0)}; // 'this'
    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_llvm_types, false);
    llvm::Function* function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, &ctx.llvm_module);
    
    // Store the destructor function in the ClassSymbol
    auto* class_symbol = ctx.symbol_table.find_class(class_name);
    if (class_symbol) {
        class_symbol->destructor_func = function;
    }
    
    return function;
}

void CodeGenerator::cg_compile_destructor_body(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name) {
    ctx.named_values.clear();
    std::string func_name = class_name + ".%dtor";
    llvm::Function* function = ctx.llvm_module.getFunction(func_name);
    if (!function) {
        log_compiler_error("Destructor signature not found: " + func_name, node->location);
    }
    
    const auto* class_symbol = ctx.symbol_table.find_class(class_name);
    if (!class_symbol) {
        log_compiler_error("Class not found for destructor: " + class_name, node->location);
    }
    const SymbolTable::ClassSymbol* this_class_info = class_symbol;
    
    ctx.scope_manager.push_scope(ScopeType::Function, func_name);
    ctx.current_function = function;

    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(ctx.llvm_context, "entry", function);
    ctx.builder.SetInsertPoint(entry_block);
    
    auto llvm_arg_it = function->arg_begin();
    VariableInfo thisVarInfo;
    thisVarInfo.alloca = create_entry_block_alloca(ctx, "this.dtor.arg", llvm_arg_it->getType());
    thisVarInfo.classInfo = this_class_info;
    ctx.builder.CreateStore(&*llvm_arg_it, thisVarInfo.alloca);
    ctx.named_values["this"] = thisVarInfo;

    // Set up field access for destructor - add each field to namedValues for direct access
    if (this_class_info && this_class_info->fieldsType) {
        llvm::Value* this_fields_ptr = ctx.builder.CreateLoad(llvm_arg_it->getType(), thisVarInfo.alloca, "this.fields.dtor");
        for (unsigned i = 0; i < this_class_info->field_names.size(); ++i) {
            const std::string& field_name = this_class_info->field_names[i];
            llvm::Type* field_llvm_type = this_class_info->fieldsType->getElementType(i);

            llvm::Value* field_ptr = ctx.builder.CreateStructGEP(this_class_info->fieldsType, this_fields_ptr, i, field_name + ".ptr.dtor");

            VariableInfo fieldVarInfo;
            fieldVarInfo.alloca = create_entry_block_alloca(ctx, field_name + ".dtor.access", field_llvm_type);
            if (i < this_class_info->field_ast_types.size()) {
                fieldVarInfo.declaredTypeNode = this_class_info->field_ast_types[i];
            }

            llvm::Value* field_val = ctx.builder.CreateLoad(field_llvm_type, field_ptr, field_name + ".val.dtor");
            ctx.builder.CreateStore(field_val, fieldVarInfo.alloca);

            if (field_llvm_type->isPointerTy() && fieldVarInfo.declaredTypeNode) {
                if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&fieldVarInfo.declaredTypeNode->name_segment)) {
                    if (auto identNode = *identNode_variant) {
                        const auto* field_class_symbol = ctx.symbol_table.find_class(identNode->name);
                        if (field_class_symbol) {
                            fieldVarInfo.classInfo = field_class_symbol;
                        }
                    }
                }
            }

            ctx.named_values[field_name] = fieldVarInfo;
        }
    }
    
    if (node->body) {
        cg_statement(node->body.value());
    }
    
    if (!ctx.builder.GetInsertBlock()->getTerminator()) {
        ctx.scope_manager.pop_scope();
        ctx.builder.CreateRetVoid();
    }
    
    if (llvm::verifyFunction(*function, &llvm::errs())) {
        log_compiler_error("Destructor function '" + func_name + "' verification failed.", node->location);
    }
    
    ctx.current_function = nullptr;
}

void CodeGenerator::cg_populate_vtable_for_class(const std::string& fq_class_name) {
    auto* class_symbol = ctx.symbol_table.find_class(fq_class_name);
    if (!class_symbol || !class_symbol->vtable_global) {
        return; // No VTable for this class
    }
    
    std::vector<llvm::Constant*> vtable_initializers;
    
    // Slot 0: Destructor
    std::string dtor_name = fq_class_name + ".%dtor";
    llvm::Function* dtor_func = ctx.llvm_module.getFunction(dtor_name);
    if (dtor_func) {
        vtable_initializers.push_back(dtor_func);
    } else {
        llvm::Type* dtor_ptr_type = class_symbol->vtable_type->getElementType(0);
        vtable_initializers.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(dtor_ptr_type)));
    }
    
    // Populate virtual method slots
    for (const auto& virtual_method_qname : class_symbol->virtual_method_order) {
        llvm::Function* method_func = ctx.llvm_module.getFunction(virtual_method_qname);
        if (method_func) {
            vtable_initializers.push_back(method_func);
        } else {
            log_compiler_error("Virtual method function not found during VTable population: " + virtual_method_qname);
        }
    }
    
    llvm::Constant* vtable_initializer = llvm::ConstantStruct::get(class_symbol->vtable_type, vtable_initializers);
    class_symbol->vtable_global->setInitializer(vtable_initializer);
}

void CodeGenerator::cg_generate_vtable_for_class(SymbolTable::ClassSymbol& class_symbol, const SymbolTable::ClassSymbol* class_symbol_const, const std::string& fq_class_name) {
    // SAFETY CHECKS
    if (!class_symbol_const) {
        log_compiler_error("Cannot generate VTable: class symbol is null for " + fq_class_name);
    }
    
    if (class_symbol_const->virtual_method_order.empty()) {
        // No virtual methods, no need for VTable
        return;
    }
    
    std::vector<llvm::Type*> vtable_slot_types;
    
    // Slot 0: Destructor (void (ptr))
    llvm::FunctionType* dtor_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(ctx.llvm_context), 
        {llvm::PointerType::get(ctx.llvm_context, 0)}, 
        false);
    vtable_slot_types.push_back(llvm::PointerType::get(dtor_type, 0));

    // Subsequent slots: Virtual methods
    for (const auto& virtual_method_qname : class_symbol_const->virtual_method_order) {
        // SAFETY CHECK: Verify method exists
        if (virtual_method_qname.empty()) {
            log_compiler_error("Empty virtual method name found in VTable for class: " + fq_class_name);
        }
        
        const auto* method_symbol = ctx.symbol_table.find_method(virtual_method_qname);
        if (!method_symbol) {
            log_compiler_error("Virtual method not found in symbol table: " + virtual_method_qname + " for class: " + fq_class_name);
        }
        
        // SAFETY CHECK: Verify method has return type
        if (!method_symbol->return_type) {
            log_compiler_error("Virtual method has no return type: " + virtual_method_qname);
        }
        
        llvm::Type* return_type = get_llvm_type(ctx, method_symbol->return_type);
        if (!return_type) {
            log_compiler_error("Cannot determine LLVM return type for virtual method: " + virtual_method_qname);
        }
        
        std::vector<llvm::Type*> param_types;
        if (!method_symbol->is_static) {
            param_types.push_back(llvm::PointerType::get(ctx.llvm_context, 0));
        }
        for (const auto& param_type_node : method_symbol->parameter_types) {
            if (!param_type_node) {
                log_compiler_error("Virtual method parameter has null type: " + virtual_method_qname);
            }
            llvm::Type* param_type = get_llvm_type(ctx, param_type_node);
            if (!param_type) {
                log_compiler_error("Cannot determine LLVM parameter type for virtual method: " + virtual_method_qname);
            }
            param_types.push_back(param_type);
        }

        llvm::FunctionType* method_func_type = llvm::FunctionType::get(return_type, param_types, false);
        vtable_slot_types.push_back(llvm::PointerType::get(method_func_type, 0));
    }
    
    if (vtable_slot_types.empty()) {
        log_compiler_error("VTable has no slots for class: " + fq_class_name);
    }
    
    std::string vtable_type_name = fq_class_name + "_VTable";
    class_symbol.vtable_type = llvm::StructType::create(ctx.llvm_context, vtable_slot_types, vtable_type_name);
    
    if (!class_symbol.vtable_type) {
        log_compiler_error("Failed to create VTable type for class: " + fq_class_name);
    }
    
    std::string vtable_global_name = fq_class_name + "_vtable_global";
    class_symbol.vtable_global = new llvm::GlobalVariable(
        ctx.llvm_module, class_symbol.vtable_type, true, 
        llvm::GlobalValue::ExternalLinkage, nullptr, vtable_global_name);
        
    if (!class_symbol.vtable_global) {
        log_compiler_error("Failed to create VTable global variable for class: " + fq_class_name);
    }
}

} // namespace Mycelium::Scripting::Lang::CodeGen