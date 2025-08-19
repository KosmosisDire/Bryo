// codegen.cpp - LLVM Code Generator with Pre-declaration Support
#include "codegen/codegen.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/Casting.h>
#include "semantic/type.hpp"
#include <iostream>

namespace Myre
{

    // === Main API ===

    std::unique_ptr<llvm::Module> CodeGenerator::generate(CompilationUnit *unit)
    {
        // Clear any previous state from prior runs
        locals.clear();
        local_types.clear();
        type_cache.clear();
        defined_types.clear();
        declared_functions.clear();
        errors.clear();
        while (!value_stack.empty())
            value_stack.pop();

        // Step 1: Declare all user-defined types (structs) recursively from the global namespace.
        // This pass ensures that any function or type can reference any other type, regardless of order.
        declare_all_types();

        // Step 2: Declare all function signatures.
        // This allows for mutual recursion and calls to functions defined later in the file.
        declare_all_functions();

        // Step 3: Generate the actual code for function bodies and global initializers.
        visit(unit);

        // Step 4: Verify the generated module for consistency.
        std::string verify_error;
        llvm::raw_string_ostream error_stream(verify_error);
        if (llvm::verifyModule(*module, &error_stream))
        {
            report_general_error("LLVM Module verification failed: " + verify_error);
        }

        return std::move(module);
    }

    // === Pre-declaration Passes ===

    void CodeGenerator::declare_all_types()
    {
        auto *global_scope = symbol_table.get_global_namespace();
        if (global_scope)
        {
            declare_all_types_in_scope(global_scope);
        }
    }

    void CodeGenerator::declare_all_types_in_scope(Scope *scope)
    {
        if (!scope)
            return;

        for (const auto &[name, symbol] : scope->symbols)
        {
            if (auto *type_sym = symbol->as<TypeSymbol>())
            {
                if (defined_types.count(type_sym) > 0)
                    continue;

                // Create an "opaque" struct type first. This is crucial for handling
                // recursive types (e.g., struct Node { Node* next; }).
                llvm::StructType *struct_type = llvm::StructType::create(*context, type_sym->get_qualified_name());
                defined_types[type_sym] = struct_type;

                // Now, define the body of the struct by resolving its member types.
                std::vector<llvm::Type *> member_types;
                for (const auto &[member_name, member_symbol] : type_sym->symbols)
                {
                    if (auto *field = member_symbol->as<VariableSymbol>())
                    {
                        member_types.push_back(get_llvm_type(field->type()));
                    }
                }
                struct_type->setBody(member_types);
            }

            // Recursively process nested scopes (e.g., namespaces)
            if (auto *nested_scope = symbol->as<Scope>())
            {
                declare_all_types_in_scope(nested_scope);
            }
        }
    }

    void CodeGenerator::declare_all_functions()
    {
        auto *global_scope = symbol_table.get_global_namespace();
        if (global_scope)
        {
            declare_all_functions_in_scope(global_scope);
        }
    }

    void CodeGenerator::declare_all_functions_in_scope(Scope *scope)
    {
        if (!scope)
            return;

        for (const auto &[name, symbol_node] : scope->symbols)
        {
            if (auto *func_sym = symbol_node->as<FunctionSymbol>())
            {
                declare_function_from_symbol(func_sym);
            }

            // Look inside types for methods
            if (auto *type_sym = symbol_node->as<TypeSymbol>())
            {
                // Iterate through type members to find methods
                for (const auto &[member_name, member_symbol] : type_sym->symbols)
                {
                    if (auto *method_sym = member_symbol->as<FunctionSymbol>())
                    {
                        declare_function_from_symbol(method_sym);
                    }
                }
            }

            // Add property getter/setter declarations
            if (auto *prop_sym = symbol_node->as<PropertySymbol>())
            {
                // Generate getter function declaration
                std::string getter_name = prop_sym->get_qualified_name() + ".get";

                // Getter takes 'this' pointer and returns property type
                std::vector<llvm::Type *> param_types;

                // Get the containing type for 'this' parameter
                if (auto *type_sym = prop_sym->parent->as<TypeSymbol>())
                {
                    auto type_it = defined_types.find(type_sym);
                    if (type_it != defined_types.end())
                    {
                        param_types.push_back(llvm::PointerType::get(*context, 0)); // Opaque pointer
                    }
                }

                llvm::Type *return_type = get_llvm_type(prop_sym->type());
                auto *func_type = llvm::FunctionType::get(return_type, param_types, false);

                llvm::Function::Create(
                    func_type, llvm::Function::ExternalLinkage, getter_name, module.get());

                declared_functions.insert(getter_name);
            }

            // Recursively process nested scopes
            if (auto *nested_scope = symbol_node->as<Scope>())
            {
                declare_all_functions_in_scope(nested_scope);
            }
        }
    }
    llvm::Function *CodeGenerator::declare_function_from_symbol(FunctionSymbol *func_symbol)
    {
        if (!func_symbol)
            return nullptr;

        const std::string &func_name = func_symbol->get_qualified_name();
        if (declared_functions.count(func_name) > 0)
        {
            return module->getFunction(func_name);
        }

        std::vector<llvm::Type *> param_types;

        // Check if this is a method (function inside a type)
        // If so, add 'this' as the first parameter
        if (auto *parent_type = func_symbol->parent->as<TypeSymbol>())
        {
            // This is a method - add 'this' pointer as first parameter
            auto type_it = defined_types.find(parent_type);
            if (type_it != defined_types.end())
            {
                param_types.push_back(llvm::PointerType::get(*context, 0)); // Opaque pointer for 'this'
            }
        }

        // Add the explicitly declared parameters
        for (const auto &param_handle : func_symbol->parameters())
        {
            auto *param_symbol = symbol_table.lookup_handle(param_handle)->as<ParameterSymbol>();
            if (!param_symbol)
            {
                report_general_error("Internal error: Parameter symbol not found");
                continue;
            }
            param_types.push_back(get_llvm_type(param_symbol->type()));
        }

        llvm::Type *return_type = get_llvm_type(func_symbol->return_type());
        auto *func_type = llvm::FunctionType::get(return_type, param_types, false);

        auto *function = llvm::Function::Create(
            func_type, llvm::Function::ExternalLinkage, func_name, module.get());

        declared_functions.insert(func_name);
        return function;
    }

    void CodeGenerator::generate_definitions(CompilationUnit *unit)
    {
        if (unit)
        {
            unit->accept(this);
        }
    }

    // === Helper Methods ===

    void CodeGenerator::debug_print_module_state(const std::string &phase)
    {
        std::cerr << "\n===== MODULE STATE: " << phase << " =====\n";

        // Print all functions in the module
        std::cerr << "Functions in module:\n";
        for (auto &F : module->functions())
        {
            std::cerr << "  Function: " << F.getName().str() << "\n";
            std::cerr << "    Type: ";
            F.getFunctionType()->print(llvm::errs());
            std::cerr << "\n";
            std::cerr << "    Num args declared: " << F.arg_size() << "\n";
            std::cerr << "    Return type: ";
            F.getReturnType()->print(llvm::errs());
            std::cerr << "\n";

            // Print argument types
            if (F.arg_size() > 0)
            {
                std::cerr << "    Arg types: ";
                for (size_t i = 0; i < F.getFunctionType()->getNumParams(); i++)
                {
                    F.getFunctionType()->getParamType(i)->print(llvm::errs());
                    std::cerr << " ";
                }
                std::cerr << "\n";
            }

            std::cerr << "    Has body: " << (!F.empty() ? "yes" : "no") << "\n";
        }

        // Print the entire module IR
        std::cerr << "\n--- Full Module IR ---\n";
        module->print(llvm::errs(), nullptr);
        std::cerr << "===== END MODULE STATE =====\n\n";
    }

    void CodeGenerator::generate_property_getter(PropertyDecl *prop_decl, TypeSymbol *type_symbol, llvm::StructType *struct_type)
    {
        if (!prop_decl || !prop_decl->variable || !prop_decl->variable->variable ||
            !prop_decl->variable->variable->name || !prop_decl->getter)
            return;

        std::string prop_name(prop_decl->variable->variable->name->text);

        // Find the property symbol
        auto *prop_symbol = type_symbol->lookup_local(prop_name)->as<PropertySymbol>();
        if (!prop_symbol)
            return;

        std::string getter_name = prop_symbol->get_qualified_name() + ".get";

        auto *getter_func = module->getFunction(getter_name);
        if (!getter_func || !getter_func->empty())
            return; // Already has a body or doesn't exist

        // Save current function context
        auto *saved_function = current_function;
        auto saved_locals = locals;
        auto saved_local_types = local_types;

        current_function = getter_func;
        locals.clear();
        local_types.clear();

        // Create entry block
        auto *entry = llvm::BasicBlock::Create(*context, "entry", getter_func);
        builder->SetInsertPoint(entry);

        // Set up 'this' parameter
        llvm::Value *this_param = getter_func->arg_begin();
        this_param->setName("this");

        // Make struct fields accessible as locals
        int field_index = 0;
        for (const auto &[member_name, member_node] : type_symbol->symbols)
        {
            if (auto *field_sym = member_node->as<VariableSymbol>())
            {
                // Create a GEP to access this field through the 'this' pointer
                auto *field_ptr = builder->CreateStructGEP(struct_type, this_param,
                                                           field_index, member_name);
                locals[field_sym] = field_ptr;
                local_types[field_sym] = get_llvm_type(field_sym->type());
                field_index++;
            }
        }

        // Generate the getter body
        if (auto *expr = std::get_if<Expression *>(&prop_decl->getter->body))
        {
            (*expr)->accept(this);
            auto *result = pop_value();
            if (result)
            {
                // Load the value if it's a pointer (but not for structs)
                if (result->getType()->isPointerTy() &&
                    !result->getType()->getContainedType(0)->isStructTy())
                {
                    auto *prop_type = get_llvm_type((*expr)->resolvedType);
                    result = builder->CreateLoad(prop_type, result);
                }
                builder->CreateRet(result);
            }
        }
        else if (auto *block = std::get_if<Block *>(&prop_decl->getter->body))
        {
            (*block)->accept(this);
            ensure_terminator();
        }

        // Restore context
        current_function = saved_function;
        locals = saved_locals;
        local_types = saved_local_types;
    }

    Scope *CodeGenerator::get_containing_scope(Node *node)
    {
        if (!node || node->containingScope.id == 0)
            return nullptr;
        return symbol_table.lookup_handle(node->containingScope)->as<Scope>();
    }

    std::string CodeGenerator::build_qualified_name(NameExpr *name_expr)
    {
        if (!name_expr || !name_expr->name)
            return "";
        return std::string(name_expr->name->text);
    }

    void CodeGenerator::push_value(llvm::Value *val)
    {
        if (val)
        {
            value_stack.push(val);
        }
        else
        {
            report_general_error("Internal error: Attempted to push a null value to the stack");
        }
    }

    llvm::Value *CodeGenerator::pop_value()
    {
        if (value_stack.empty())
        {
            report_general_error("Internal error: Attempted to pop from an empty value stack");
            return nullptr;
        }
        auto *val = value_stack.top();
        value_stack.pop();
        return val;
    }

    llvm::Type *CodeGenerator::get_llvm_type(TypePtr type)
    {
        if (!type)
        {
            // Default to void if a null type is encountered, but this indicates a semantic analysis issue.
            return llvm::Type::getVoidTy(*context);
        }

        auto it = type_cache.find(type);
        if (it != type_cache.end())
        {
            return it->second;
        }

        llvm::Type *llvm_type = nullptr;

        assert(!type->is<UnresolvedType>() && "Unresolved types should not reach code generation");

        if (auto *prim = std::get_if<PrimitiveType>(&type->value))
        {
            switch (prim->kind)
            {
            case PrimitiveType::I32:
                llvm_type = llvm::Type::getInt32Ty(*context);
                break;
            case PrimitiveType::I64:
                llvm_type = llvm::Type::getInt64Ty(*context);
                break;
            case PrimitiveType::F32:
                llvm_type = llvm::Type::getFloatTy(*context);
                break;
            case PrimitiveType::F64:
                llvm_type = llvm::Type::getDoubleTy(*context);
                break;
            case PrimitiveType::Bool:
                llvm_type = llvm::Type::getInt1Ty(*context);
                break;
            case PrimitiveType::Void:
                llvm_type = llvm::Type::getVoidTy(*context);
                break;
            default:
                report_general_error("Unsupported primitive type for codegen");
                llvm_type = llvm::Type::getVoidTy(*context);
                break;
            }
        }
        else if (auto *ref = std::get_if<TypeReference>(&type->value))
        {
            if (ref->definition)
            {
                auto *type_sym = ref->definition->as<TypeSymbol>();
                if (type_sym)
                {
                    auto defined_it = defined_types.find(type_sym);
                    if (defined_it != defined_types.end())
                    {
                        llvm_type = defined_it->second;
                    }
                    else
                    {
                        report_general_error("Internal error: Type '" + type_sym->name() + "' was not pre-declared.");
                    }
                }
            }
        }
        else
        {
            report_general_error("Unsupported type kind for codegen");
        }

        if (!llvm_type)
        {
            llvm_type = llvm::Type::getVoidTy(*context);
        }

        type_cache[type] = llvm_type;
        return llvm_type;
    }

    llvm::Value *CodeGenerator::create_constant(LiteralExpr *literal)
    {
        if (!literal)
            return nullptr;
        std::string text(literal->value);

        switch (literal->kind)
        {
        case LiteralKind::I32:
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), std::stoll(text));
        case LiteralKind::F32:
            return llvm::ConstantFP::get(llvm::Type::getFloatTy(*context), std::stod(text));
        case LiteralKind::Bool:
            return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), (text == "true"));
        default:
            report_error(literal, "Unsupported literal type");
            return nullptr;
        }
    }

    void CodeGenerator::ensure_terminator()
    {
        auto *bb = builder->GetInsertBlock();
        if (bb && !bb->getTerminator())
        {
            if (current_function)
            {
                auto *ret_type = current_function->getReturnType();
                if (ret_type->isVoidTy())
                {
                    builder->CreateRetVoid();
                }
                else
                {
                    // Functions returning non-void must have explicit return statements.
                    // An implicit return would be undefined behavior.
                    builder->CreateUnreachable();
                }
            }
        }
    }

    void CodeGenerator::report_error(const Node *node, const std::string &message)
    {
        errors.push_back({message, node ? node->location : SourceRange{}});
    }

    void CodeGenerator::report_general_error(const std::string &message)
    {
        errors.push_back({message, {}});
    }

    // === Visitor Implementations ===

    // --- Root and Declarations ---

    void CodeGenerator::visit(CompilationUnit *node)
    {
        if (!node)
            return;
        for (auto *stmt : node->topLevelStatements)
        {
            if (stmt)
                stmt->accept(this);
        }
    }

    void CodeGenerator::visit(NamespaceDecl *node)
    {
        if (!node || !node->body)
            return;
        for (auto *stmt : *node->body)
        {
            if (stmt)
                stmt->accept(this);
        }
    }

    void CodeGenerator::visit(TypeDecl *node)
    {
        if (!node || !node->name)
            return;

        // Find the type symbol
        auto *scope = get_containing_scope(node);
        if (!scope)
            return;

        auto *type_symbol = scope->lookup(node->name->text)->as<TypeSymbol>();
        if (!type_symbol)
            return;

        // Get the LLVM struct type
        auto type_it = defined_types.find(type_symbol);
        if (type_it == defined_types.end())
            return;

        llvm::StructType *struct_type = llvm::cast<llvm::StructType>(type_it->second);

        // Process method declarations to generate their bodies
        for (auto *member : node->members)
        {
            if (auto *func_decl = member->as<FunctionDecl>())
            {
                // Get the already-declared function
                auto *func_symbol = symbol_table.lookup_handle(func_decl->functionSymbol)->as<FunctionSymbol>();
                if (!func_symbol)
                    continue;

                auto *function = module->getFunction(func_symbol->get_qualified_name());
                if (!function || !function->empty())
                    continue; // Already has a body or doesn't exist

                // Save current context
                auto *saved_function = current_function;
                auto saved_locals = locals;
                auto saved_local_types = local_types;

                current_function = function;
                locals.clear();
                local_types.clear();

                auto *entry = llvm::BasicBlock::Create(*context, "entry", function);
                builder->SetInsertPoint(entry);

                // Set up 'this' parameter (first argument)
                llvm::Value *this_param = function->arg_begin();
                this_param->setName("this");

                // Make struct fields accessible as locals through 'this'
                int field_index = 0;
                for (const auto &[member_name, member_node] : type_symbol->symbols)
                {
                    if (auto *field_sym = member_node->as<VariableSymbol>())
                    {
                        auto *field_ptr = builder->CreateStructGEP(struct_type, this_param,
                                                                   field_index, member_name);
                        locals[field_sym] = field_ptr;
                        local_types[field_sym] = get_llvm_type(field_sym->type());
                        field_index++;
                    }
                }

                // Handle other parameters
                size_t llvm_param_index = 1; // Start from 1 since 0 is 'this'
                for (const auto &param_symbol_handle : func_symbol->parameters())
                {
                    auto *param_symbol = symbol_table.lookup_handle(param_symbol_handle)->as<ParameterSymbol>();
                    if (!param_symbol)
                        continue;

                    auto *alloca = builder->CreateAlloca(get_llvm_type(param_symbol->type()), nullptr, param_symbol->name());
                    auto *param_value = function->arg_begin() + llvm_param_index;
                    builder->CreateStore(param_value, alloca);

                    locals[param_symbol] = alloca;
                    local_types[param_symbol] = get_llvm_type(param_symbol->type());
                    llvm_param_index++;
                }

                // Generate the method body
                func_decl->body->accept(this);
                ensure_terminator();

                // Restore context
                current_function = saved_function;
                locals = saved_locals;
                local_types = saved_local_types;
            }
            else if (auto *prop_decl = member->as<PropertyDecl>())
            {
                // Generate getter function body
                if (prop_decl->getter)
                {
                    generate_property_getter(prop_decl, type_symbol, struct_type);
                }
                // TODO: Handle setters if needed
                if (prop_decl->setter)
                {
                    report_error(prop_decl->setter, "Setter generation not implemented");
                }
            }
        }
    }
    void CodeGenerator::visit(FunctionDecl *node)
    {
        if (!node || !node->name)
            return;

        // Skip if this is a method inside a type - it will be handled by visit(TypeDecl)
        auto *parent_scope = get_containing_scope(node);
        if (parent_scope && parent_scope->scope_as<TypeSymbol>())
            return; // This is a method, handled elsewhere

        auto *func_symbol = symbol_table.lookup_handle(node->functionSymbol)->as<FunctionSymbol>();
        if (!func_symbol)
        {
            report_error(node, "Function symbol not found for '" + std::string(node->name->text) + "'");
            return;
        }

        auto *function = module->getFunction(func_symbol->get_qualified_name());
        if (!function || !function->empty())
            return; // Already declared or has a body
        if (!node->body)
            return; // Abstract function, no body to generate

        current_function = function;
        locals.clear();
        local_types.clear();

        auto *entry = llvm::BasicBlock::Create(*context, "entry", function);
        builder->SetInsertPoint(entry);

        size_t param_index = 0;
        for (const auto &param_symbol_handle : func_symbol->parameters())
        {
            auto *param_symbol = symbol_table.lookup_handle(param_symbol_handle)->as<ParameterSymbol>();
            if (!param_symbol)
            {
                report_error(node, "Parameter symbol not found for function '" + func_symbol->name() + "'");
                param_index++;
                continue;
            }

            // Create alloca for parameter
            auto *alloca = builder->CreateAlloca(get_llvm_type(param_symbol->type()), nullptr, param_symbol->name());
            auto *param_value = function->arg_begin() + param_index;
            builder->CreateStore(param_value, alloca);

            locals[param_symbol] = alloca;
            local_types[param_symbol] = get_llvm_type(param_symbol->type());
            param_index++;
        }

        node->body->accept(this);
        ensure_terminator();
        current_function = nullptr;
    }

    void CodeGenerator::visit(VariableDecl *node)
    {
        if (!node || !node->variable || !node->variable->name)
            return;

        auto *parent_scope = get_containing_scope(node);
        if (!parent_scope)
            return;
        auto *var_symbol = parent_scope->lookup(node->variable->name->text);
        if (!var_symbol)
        {
            report_error(node, "Variable symbol not found for '" + std::string(node->variable->name->text) + "'");
            return;
        }

        auto *typed_symbol = var_symbol->as<TypedSymbol>();
        if (!typed_symbol)
            return;

        llvm::Type *llvm_type = get_llvm_type(typed_symbol->type());
        if (llvm_type->isVoidTy())
        {
            report_error(node, "Cannot declare a variable of type 'void'");
            return;
        }

        auto *alloca = builder->CreateAlloca(llvm_type, nullptr, node->variable->name->text);
        locals[var_symbol] = alloca;
        local_types[var_symbol] = llvm_type;

        if (node->initializer)
        {
            node->initializer->accept(this);
            auto *init_value = pop_value();

            if (init_value)
            {
                // If the initializer was a 'new' expression, it returned a pointer to the temp memory.
                // We need to load from that temp memory and store into our variable's memory.
                if (llvm::isa<llvm::AllocaInst>(init_value) && llvm_type->isStructTy())
                {
                    builder->CreateStore(builder->CreateLoad(llvm_type, init_value), alloca);
                }
                else
                {
                    builder->CreateStore(init_value, alloca);
                }
            }
        }
    }

    void CodeGenerator::visit(PropertyDecl *node)
    {
        // Properties are handled in visit(TypeDecl) where we have access to the type context
        // This visitor is for standalone property declarations, which don't exist in this language
    }

    void CodeGenerator::visit(ParameterDecl *node)
    {
        // Handled in visit(FunctionDecl*).
    }

    // --- Statements ---

    void CodeGenerator::visit(Block *node)
    {
        if (!node)
            return;
        for (auto *stmt : node->statements)
        {
            if (stmt)
                stmt->accept(this);
        }
    }

    void CodeGenerator::visit(ExpressionStmt *node)
    {
        if (!node || !node->expression)
            return;
        node->expression->accept(this);
        // Pop and discard the result of the expression.
        if (!value_stack.empty())
        {
            pop_value();
        }
    }

    void CodeGenerator::visit(ReturnStmt *node)
    {
        if (!node)
            return;

        if (node->value)
        {
            node->value->accept(this);
            auto *ret_value = pop_value();
            if (ret_value)
            {
                // Check if we have a pointer and need to load the value
                if (ret_value->getType()->isPointerTy() && node->value->resolvedType)
                {
                    llvm::Type *value_type = get_llvm_type(node->value->resolvedType);

                    // For return statements, we need to load the value from memory
                    // This includes all types - primitives and structs
                    // The function returns by value, so we need the actual value, not a pointer
                    ret_value = builder->CreateLoad(value_type, ret_value, "ret.load");
                }

                builder->CreateRet(ret_value);
            }
        }
        else
        {
            builder->CreateRetVoid();
        }
    }

    // --- Expressions ---
    void CodeGenerator::visit(BinaryExpr *node)
    {
        if (!node || !node->left || !node->right)
            return;

        node->left->accept(this);
        auto *left = pop_value();
        if (!left)
            return;

        node->right->accept(this);
        auto *right = pop_value();
        if (!right)
            return;

        // If operands are pointers to values, load them
        // We need to check the resolved types from the AST nodes
        if (left->getType()->isPointerTy())
        {
            llvm::Type *left_type = get_llvm_type(node->left->resolvedType);
            if (!left_type->isStructTy()) // Don't load struct pointers
            {
                left = builder->CreateLoad(left_type, left, "load_left");
            }
        }

        if (right->getType()->isPointerTy())
        {
            llvm::Type *right_type = get_llvm_type(node->right->resolvedType);
            if (!right_type->isStructTy()) // Don't load struct pointers
            {
                right = builder->CreateLoad(right_type, right, "load_right");
            }
        }

        llvm::Value *result = nullptr;
        bool is_float = left->getType()->isFloatingPointTy();

        switch (node->op)
        {
        case BinaryOperatorKind::Add:
            result = is_float ? builder->CreateFAdd(left, right, "addtmp") : builder->CreateAdd(left, right, "addtmp");
            break;
        case BinaryOperatorKind::Subtract:
            result = is_float ? builder->CreateFSub(left, right, "subtmp") : builder->CreateSub(left, right, "subtmp");
            break;
        case BinaryOperatorKind::Multiply:
            result = is_float ? builder->CreateFMul(left, right, "multmp") : builder->CreateMul(left, right, "multmp");
            break;
        case BinaryOperatorKind::Divide:
            result = is_float ? builder->CreateFDiv(left, right, "divtmp") : builder->CreateSDiv(left, right, "divtmp");
            break;
        case BinaryOperatorKind::Equals:
            result = is_float ? builder->CreateFCmpOEQ(left, right, "eqtmp") : builder->CreateICmpEQ(left, right, "eqtmp");
            break;
        case BinaryOperatorKind::NotEquals:
            result = is_float ? builder->CreateFCmpONE(left, right, "netmp") : builder->CreateICmpNE(left, right, "netmp");
            break;
        case BinaryOperatorKind::GreaterThan:
            result = is_float ? builder->CreateFCmpOGT(left, right, "gttmp") : builder->CreateICmpSGT(left, right, "gttmp");
            break;
        case BinaryOperatorKind::LessThan:
            result = is_float ? builder->CreateFCmpOLT(left, right, "lttmp") : builder->CreateICmpSLT(left, right, "lttmp");
            break;
        case BinaryOperatorKind::GreaterThanOrEqual:
            result = is_float ? builder->CreateFCmpOGE(left, right, "getmp") : builder->CreateICmpSGE(left, right, "getmp");
            break;
        case BinaryOperatorKind::LessThanOrEqual:
            result = is_float ? builder->CreateFCmpOLE(left, right, "letmp") : builder->CreateICmpSLE(left, right, "letmp");
            break;
        default:
            report_error(node, "Unsupported binary operator");
            break;
        }

        if (result)
            push_value(result);
    }

    void CodeGenerator::visit(UnaryExpr *node)
    {
        if (!node || !node->operand)
            return;
        node->operand->accept(this);
        auto *operand = pop_value();
        if (!operand)
            return;

        llvm::Value *result = nullptr;
        switch (node->op)
        {
        case UnaryOperatorKind::Minus:
            result = operand->getType()->isFloatingPointTy() ? builder->CreateFNeg(operand, "negtmp") : builder->CreateNeg(operand, "negtmp");
            break;
        case UnaryOperatorKind::Not:
            result = builder->CreateNot(operand, "nottmp");
            break;
        default:
            report_error(node, "Unsupported unary operator");
            break;
        }
        if (result)
            push_value(result);
    }

    void CodeGenerator::visit(AssignmentExpr *node)
    {
        if (!node || !node->target || !node->value)
            return;

        // Handle member access assignment
        if (auto *member_access = node->target->as<MemberAccessExpr>())
        {
            member_access->accept(this);
            auto *member_ptr = pop_value();
            if (!member_ptr)
                return;

            node->value->accept(this);
            auto *value = pop_value();
            if (!value)
                return;

            builder->CreateStore(value, member_ptr);
            push_value(value); // Assignments are expressions
            return;
        }

        // Handle regular variable assignment
        auto *name_expr = node->target->as<NameExpr>();
        if (!name_expr)
        {
            report_error(node->target, "Assignment target must be an identifier or member access");
            return;
        }

        auto *parent_scope = get_containing_scope(node->target);
        if (!parent_scope)
            return;
        auto *var_symbol = parent_scope->lookup(build_qualified_name(name_expr));
        if (!var_symbol)
        {
            report_error(node->target, "Variable not found");
            return;
        }

        auto it = locals.find(var_symbol);
        if (it == locals.end())
        {
            report_error(node->target, "Variable not found in local scope");
            return;
        }
        auto *alloca = it->second;

        node->value->accept(this);
        auto *value = pop_value();
        if (!value)
            return;

        builder->CreateStore(value, alloca);
        push_value(value); // Assignments are expressions
    }

    void CodeGenerator::visit(CallExpr *node)
    {
        if (!node || !node->callee)
            return;

        llvm::Function *callee_func = nullptr;
        llvm::Value *this_ptr = nullptr;

        // Handle method calls (callee is MemberAccessExpr)
        if (auto *member_expr = node->callee->as<MemberAccessExpr>())
        {
            // Get the object (this pointer)
            member_expr->object->accept(this);
            this_ptr = pop_value();

            // Get the method name and look up the function
            std::string method_name;
            auto obj_type = member_expr->object->resolvedType;
            if (auto *type_ref = std::get_if<TypeReference>(&obj_type->value))
            {
                if (auto *type_sym = type_ref->definition->as<TypeSymbol>())
                {
                    method_name = type_sym->get_qualified_name() + "." + std::string(member_expr->member->text);
                    callee_func = module->getFunction(method_name);

                    if (!callee_func)
                    {
                        report_error(node->callee, "Unknown method referenced: " + method_name);
                        return;
                    }
                }
            }

            if (!callee_func)
            {
                report_error(node->callee, "Could not resolve method call");
                return;
            }
        }
        // Handle regular function calls (callee is NameExpr)
        else if (auto *name_expr = node->callee->as<NameExpr>())
        {
            std::string func_name = build_qualified_name(name_expr);
            callee_func = module->getFunction(func_name);

            // If not found as a global function, check if it's an implicit method call
            if (!callee_func && current_function)
            {
                // Check if we're inside a method (function has 'this' as first parameter)
                if (current_function->arg_size() > 0)
                {
                    auto *first_param = current_function->arg_begin();
                    if (first_param->getName() == "this")
                    {
                        // We're in a method - try to find the function as a method of the same type
                        std::string current_func_name = current_function->getName().str();
                        size_t dot_pos = current_func_name.find('.');
                        if (dot_pos != std::string::npos)
                        {
                            std::string type_name = current_func_name.substr(0, dot_pos);
                            std::string method_name = type_name + "." + func_name;

                            callee_func = module->getFunction(method_name);
                            if (callee_func)
                            {
                                // Use the current function's 'this' parameter
                                this_ptr = first_param;
                            }
                        }
                    }
                }
            }

            if (!callee_func)
            {
                report_error(node->callee, "Unknown function referenced: " + func_name);
                return;
            }
        }
        else
        {
            report_error(node->callee, "Function call target must be an identifier or member access");
            return;
        }

        // Collect arguments
        std::vector<llvm::Value *> args;

        // For method calls, add 'this' as the first argument
        if (this_ptr)
        {
            args.push_back(this_ptr);
        }

        // Add regular arguments
        size_t param_index = this_ptr ? 1 : 0; // Start from 1 if we have 'this'
        for (auto *arg : node->arguments)
        {
            arg->accept(this);
            llvm::Value *arg_value = pop_value();

            // Load from pointer if necessary
            if (arg_value && arg_value->getType()->isPointerTy())
            {
                // Get the expected parameter type from the function signature
                if (param_index < callee_func->arg_size())
                {
                    llvm::Type *expected_type = callee_func->getFunctionType()->getParamType(param_index);

                    // If we have a pointer but the function expects a value, load it
                    if (!expected_type->isPointerTy() && arg->resolvedType)
                    {
                        llvm::Type *arg_llvm_type = get_llvm_type(arg->resolvedType);
                        // Load the value - this includes structs when passed by value
                        arg_value = builder->CreateLoad(arg_llvm_type, arg_value, "arg.load");
                    }
                }
            }

            args.push_back(arg_value);
            param_index++;
        }

        if (args.size() != callee_func->arg_size())
        {
            report_error(node, "Incorrect number of arguments");
            return;
        }

        auto *call_value = builder->CreateCall(callee_func, args,
                                               callee_func->getReturnType()->isVoidTy() ? "" : "calltmp");

        if (!callee_func->getReturnType()->isVoidTy())
        {
            push_value(call_value);
        }
    }

    void CodeGenerator::visit(NameExpr *node)
    {
        if (!node || !node->name)
            return;
        auto *parent_scope = get_containing_scope(node);
        if (!parent_scope)
            return;

        auto var_name = build_qualified_name(node);
        auto *var_symbol = parent_scope->lookup(var_name);
        if (!var_symbol)
        {
            report_error(node, "Identifier not found: " + var_name);
            return;
        }

        auto it = locals.find(var_symbol);
        if (it == locals.end())
        {
            report_error(node, "Variable not found in local scope: " + var_name);
            return;
        }
        auto *alloca = it->second;

        // Load the value from the memory location (alloca) and push it to the stack.
        // For structs, we push the pointer (alloca) itself, as we operate on them by reference.
        if (get_llvm_type(node->resolvedType)->isStructTy())
        {
            push_value(alloca);
        }
        else
        {
            push_value(builder->CreateLoad(local_types[var_symbol], alloca, var_name));
        }
    }

    void CodeGenerator::visit(LiteralExpr *node)
    {
        auto *constant = create_constant(node);
        if (constant)
        {
            push_value(constant);
        }
    }

    void CodeGenerator::visit(Identifier *node)
    {
        // Identifiers are typically handled by NameExpr which contains identifier parts.
        // Individual identifier nodes usually don't generate code directly.
    }

    void CodeGenerator::visit(NewExpr *node)
    {
        if (!node || !node->resolvedType)
            return;

        llvm::Type *llvm_type = get_llvm_type(node->resolvedType);
        if (!llvm_type || !llvm_type->isStructTy())
        {
            report_error(node, "'new' can only be used with user-defined struct types.");
            return;
        }

        // Create a temporary, anonymous variable on the stack for this expression
        auto *temp_alloca = builder->CreateAlloca(llvm_type, nullptr, "new.tmp");

        // If there are constructor arguments, initialize the fields
        if (node->arguments.size() > 0)
        {
            // Get the struct type information
            if (auto *type_ref = std::get_if<TypeReference>(&node->resolvedType->value))
            {
                if (auto *type_sym = type_ref->definition->as<TypeSymbol>())
                {
                    // Process each argument and assign to corresponding field
                    int field_index = 0;
                    for (auto *arg : node->arguments)
                    {
                        if (field_index >= llvm_type->getStructNumElements())
                        {
                            report_error(node, "Too many constructor arguments");
                            break;
                        }

                        arg->accept(this);
                        auto *arg_value = pop_value();

                        if (arg_value)
                        {
                            // Get pointer to the field
                            auto *field_ptr = builder->CreateStructGEP(llvm_type, temp_alloca,
                                                                       field_index, "field.init");

                            // Store the value in the field
                            builder->CreateStore(arg_value, field_ptr);
                        }
                        field_index++;
                    }
                }
            }
        }
        else
        {
            // Zero-initialize if no arguments
            llvm::DataLayout dl = module->getDataLayout();
            uint64_t type_size = dl.getTypeAllocSize(llvm_type);
            builder->CreateMemSet(temp_alloca, llvm::ConstantInt::get(builder->getInt8Ty(), 0),
                                  type_size, llvm::MaybeAlign(4));
        }

        // The result is the pointer to the newly initialized temporary memory
        push_value(temp_alloca);
    }

    // --- Base/Error/Unimplemented Visitors ---

    void CodeGenerator::visit(Node *node) {}
    void CodeGenerator::visit(Expression *node) { report_error(node, "Codegen for this expression type is not yet implemented."); }
    void CodeGenerator::visit(Statement *node) { report_error(node, "Codegen for this statement type is not yet implemented."); }
    void CodeGenerator::visit(Declaration *node) { report_error(node, "Codegen for this declaration type is not yet implemented."); }
    void CodeGenerator::visit(ErrorExpression *n) { report_error(n, "Error expression: " + std::string(n->message)); }
    void CodeGenerator::visit(ErrorStatement *n) { report_error(n, "Error statement: " + std::string(n->message)); }
    void CodeGenerator::visit(ArrayLiteralExpr *n) { report_error(n, "Array literals not yet supported."); }

    void CodeGenerator::visit(MemberAccessExpr *node)
    {
        if (!node || !node->object || !node->member)
            return;

        // Get the struct pointer or value
        node->object->accept(this);
        auto *struct_val = pop_value();
        if (!struct_val)
            return;

        // Get the object's resolved type to find the struct type
        auto obj_type = node->object->resolvedType;
        if (!obj_type)
        {
            report_error(node, "Object type not resolved");
            return;
        }

        // Get the LLVM struct type from our type system
        llvm::Type *struct_type = get_llvm_type(obj_type);
        if (!struct_type->isStructTy())
        {
            report_error(node, "Member access on non-struct type");
            return;
        }

        // If we have a struct value (not a pointer), we need to store it temporarily
        // to get a pointer for member access or property getter calls
        llvm::Value *struct_ptr = struct_val;
        if (!struct_val->getType()->isPointerTy())
        {
            // We have a struct value, need to create a temporary alloca to store it
            auto *temp_alloca = builder->CreateAlloca(struct_type, nullptr, "struct.temp");
            builder->CreateStore(struct_val, temp_alloca);
            struct_ptr = temp_alloca;
        }

        // Find member and determine its type
        if (auto *type_ref = std::get_if<TypeReference>(&obj_type->value))
        {
            if (auto *type_sym = type_ref->definition->as<TypeSymbol>())
            {
                std::string member_name(node->member->text);
                auto *member_symbol = type_sym->lookup_local(member_name);

                if (!member_symbol)
                {
                    report_error(node, "Member not found: " + member_name);
                    return;
                }

                // Handle different member types
                if (auto *var_sym = member_symbol->as<VariableSymbol>())
                {
                    // Field access - use GEP
                    int member_index = 0;
                    for (const auto &[name, sym_node] : type_sym->symbols)
                    {
                        if (sym_node->as<VariableSymbol>())
                        {
                            if (name == member_name)
                                break;
                            member_index++;
                        }
                    }

                    auto *member_ptr = builder->CreateStructGEP(struct_type, struct_ptr,
                                                                member_index, member_name);
                    push_value(member_ptr);
                }
                else if (auto *prop_sym = member_symbol->as<PropertySymbol>())
                {
                    // Property access - call getter
                    std::string getter_name = prop_sym->get_qualified_name() + ".get";

                    auto *getter_func = module->getFunction(getter_name);
                    if (!getter_func)
                    {
                        report_error(node, "Property getter not found: " + getter_name);
                        return;
                    }

                    // Call getter with 'this' pointer as argument
                    // struct_ptr is now guaranteed to be a pointer
                    auto *result = builder->CreateCall(getter_func, {struct_ptr}, member_name);
                    push_value(result);
                }
                else if (auto *func_sym = member_symbol->as<FunctionSymbol>())
                {
                    // Method access - push function pointer for later invocation
                    std::string method_name = func_sym->get_qualified_name();
                    auto *method_func = module->getFunction(method_name);
                    if (!method_func)
                    {
                        report_error(node, "Method not found: " + method_name);
                        return;
                    }
                    push_value(method_func);
                    // Note: For method calls, the CallExpr visitor would need to handle
                    // passing 'this' as the first argument
                }
                else
                {
                    report_error(node, "Unsupported member type: " + member_name);
                }
            }
        }
        else
        {
            report_error(node, "Member access on non-user-defined type");
        }
    }

    void CodeGenerator::visit(IndexerExpr *n) { report_error(n, "Indexer expressions not yet supported."); }
    void CodeGenerator::visit(CastExpr *n) { report_error(n, "Cast expressions not yet supported."); }
    void CodeGenerator::visit(ThisExpr *n) { report_error(n, "'this' expressions not yet supported."); }
    void CodeGenerator::visit(LambdaExpr *n) { report_error(n, "Lambda expressions not yet supported."); }
    void CodeGenerator::visit(ConditionalExpr *n) { report_error(n, "Conditional (ternary) expressions not yet supported."); }
    void CodeGenerator::visit(TypeOfExpr *n) { report_error(n, "'typeof' not yet supported."); }
    void CodeGenerator::visit(SizeOfExpr *n) { report_error(n, "'sizeof' not yet supported."); }

    void CodeGenerator::visit(IfExpr *node)
    {
        if (!node || !node->condition || !node->thenBranch)
            return;

        node->condition->accept(this);
        auto *cond_value = pop_value();

        auto *then_bb = llvm::BasicBlock::Create(*context, "if.then", current_function);
        auto *else_bb = node->elseBranch ? llvm::BasicBlock::Create(*context, "if.else", current_function) : nullptr;
        auto *merge_bb = llvm::BasicBlock::Create(*context, "if.merge", current_function);

        builder->CreateCondBr(cond_value, then_bb, else_bb ? else_bb : merge_bb);

        // Then branch
        builder->SetInsertPoint(then_bb);
        node->thenBranch->accept(this);
        if (!builder->GetInsertBlock()->getTerminator())
        {
            builder->CreateBr(merge_bb);
        }

        // Else branch
        if (else_bb)
        {
            builder->SetInsertPoint(else_bb);
            node->elseBranch->accept(this);
            if (!builder->GetInsertBlock()->getTerminator())
            {
                builder->CreateBr(merge_bb);
            }
        }

        builder->SetInsertPoint(merge_bb);
    }

    void CodeGenerator::visit(BreakStmt *n) { report_error(n, "'break' not yet supported."); }
    void CodeGenerator::visit(ContinueStmt *n) { report_error(n, "'continue' not yet supported."); }

    void CodeGenerator::visit(WhileStmt *node)
    {
        if (!node || !node->condition || !node->body)
            return;

        auto *loop_cond = llvm::BasicBlock::Create(*context, "while.cond", current_function);
        auto *loop_body = llvm::BasicBlock::Create(*context, "while.body", current_function);
        auto *loop_exit = llvm::BasicBlock::Create(*context, "while.exit", current_function);

        // Jump to condition check
        builder->CreateBr(loop_cond);

        // Condition block
        builder->SetInsertPoint(loop_cond);
        node->condition->accept(this);
        auto *cond_value = pop_value();
        builder->CreateCondBr(cond_value, loop_body, loop_exit);

        // Body block
        builder->SetInsertPoint(loop_body);
        node->body->accept(this);
        builder->CreateBr(loop_cond);

        // Continue with exit block
        builder->SetInsertPoint(loop_exit);
    }

    void CodeGenerator::visit(ForStmt *n) { report_error(n, "For loops not yet supported."); }
    void CodeGenerator::visit(UsingDirective *n) { /* No codegen needed */ }
    void CodeGenerator::visit(ConstructorDecl *n) { report_error(n, "Constructors not yet supported."); }
    void CodeGenerator::visit(PropertyAccessor *n) { report_error(n, "Properties not yet supported."); }
    void CodeGenerator::visit(EnumCaseDecl *n) { /* No codegen needed */ }
    void CodeGenerator::visit(ArrayTypeExpr *n) { /* Type expressions are not executed */ }
    void CodeGenerator::visit(FunctionTypeExpr *n) { /* Type expressions are not executed */ }

} // namespace Myre