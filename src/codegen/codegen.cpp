#include "codegen/codegen.hpp"
#include "codegen/command_processor.hpp"
#include "ast/ast_rtti.hpp"
#include <iostream>

namespace Mycelium::Scripting::Lang {

CodeGenerator::CodeGenerator(SymbolTable& table) 
    : symbol_table_(table), current_value_(ValueRef::invalid()) {
}

void CodeGenerator::visit(CompilationUnitNode* node) {
    if (!node) return;
    
    std::cout << "CompilationUnitNode: Found " << node->statements.size << " statements" << std::endl;
    
    // Visit all statements in the compilation unit
    for (int i = 0; i < node->statements.size; ++i) {
        auto* stmt = node->statements[i];
        
        std::cout << "Processing statement " << i << " of type: " << get_node_type_name(stmt) << " (ID " << (int)stmt->typeId << ")" << std::endl;
        
        // Only process function declarations for now
        if (stmt->is_a<FunctionDeclarationNode>()) {
            std::cout << "Found FunctionDeclarationNode at index " << i << std::endl;
            stmt->accept(this);
        } else if (stmt->is_a<ClassDeclarationNode>()) {
            std::cout << "Skipping ClassDeclarationNode at index " << i << std::endl;
        } else {
            std::cout << "Processing other statement type at index " << i << std::endl;
            stmt->accept(this);
        }
    }
}

void CodeGenerator::visit(LiteralExpressionNode* node) {
    if (!node || !ir_builder_ || !node->token) return;
    
    // Handle different literal types
    switch (node->kind) {
        case LiteralKind::Integer: {
            std::string token_text = std::string(node->token->text);
            int32_t value = std::stoi(token_text);
            current_value_ = ir_builder_->const_i32(value);
            break;
        }
        case LiteralKind::Boolean: {
            std::string token_text = std::string(node->token->text);
            bool value = (token_text == "true");
            current_value_ = ir_builder_->const_bool(value);
            break;
        }
        default:
            std::cerr << "Unsupported literal type" << std::endl;
            current_value_ = ValueRef::invalid();
            break;
    }
}

void CodeGenerator::visit(BinaryExpressionNode* node) {
    if (!node || !ir_builder_) return;
    
    // Visit left operand
    node->left->accept(this);
    ValueRef lhs = current_value_;
    
    // Visit right operand
    node->right->accept(this);
    ValueRef rhs = current_value_;
    
    // Generate the binary operation
    switch (node->opKind) {
        case BinaryOperatorKind::Add:
            current_value_ = ir_builder_->add(lhs, rhs);
            break;
        case BinaryOperatorKind::Subtract:
            current_value_ = ir_builder_->sub(lhs, rhs);
            break;
        case BinaryOperatorKind::Multiply:
            current_value_ = ir_builder_->mul(lhs, rhs);
            break;
        case BinaryOperatorKind::Divide:
            current_value_ = ir_builder_->div(lhs, rhs);
            break;
        default:
            std::cerr << "Unsupported binary operation" << std::endl;
            current_value_ = ValueRef::invalid();
            break;
    }
}

void CodeGenerator::visit(LocalVariableDeclarationNode* node) {
    if (!node || !ir_builder_) return;
    
    // Process all variable declarators in this statement
    for (int i = 0; i < node->declarators.size; ++i) {
        auto* var_decl = node->declarators[i];
        if (!var_decl || !var_decl->name) continue;
        
        // For now, assume all variables are i32
        ValueRef alloca_ref = ir_builder_->alloca(IRType::i32());
        
        // Store the variable in our local variables map
        std::string var_name = std::string(var_decl->name->name);
        local_vars_[var_name] = alloca_ref;
        
        // If there's an initializer, generate code for it and store the value
        if (var_decl->initializer) {
            var_decl->initializer->accept(this);
            if (current_value_.is_valid()) {
                ir_builder_->store(current_value_, alloca_ref);
            }
        }
    }
}

void CodeGenerator::visit(IdentifierExpressionNode* node) {
    if (!node || !ir_builder_ || !node->identifier) return;
    
    // Look up the variable in our local variables
    std::string var_name = std::string(node->identifier->name);
    auto it = local_vars_.find(var_name);
    if (it != local_vars_.end()) {
        // Load the value from the variable
        current_value_ = ir_builder_->load(it->second, IRType::i32());
    } else {
        std::cerr << "Unknown variable: " << var_name << std::endl;
        current_value_ = ValueRef::invalid();
    }
}

void CodeGenerator::visit(ReturnStatementNode* node) {
    if (!node || !ir_builder_) return;
    
    if (node->expression) {
        // Generate code for the return expression
        node->expression->accept(this);
        if (current_value_.is_valid()) {
            ir_builder_->ret(current_value_);
        }
    } else {
        // Void return
        ir_builder_->ret_void();
    }
}

void CodeGenerator::visit(FunctionDeclarationNode* node) {
    if (!node || !ir_builder_) {
        std::cout << "FunctionDeclarationNode: null node or null builder" << std::endl;
        return;
    }
    
    if (!node->name) {
        std::cout << "FunctionDeclarationNode: null name" << std::endl;
        return;
    }
    
    // Determine return type
    IRType return_type = IRType::void_(); // Default to void
    if (node->returnType) {
        // TODO: Parse return type properly - for now just default to void
        return_type = IRType::void_();
    }
    
    // Begin the function
    std::string func_name = std::string(node->name->name);
    std::cout << "Processing function: '" << func_name << "'" << std::endl;
    ir_builder_->function_begin(func_name, return_type);
    
    // Clear local variables for this function
    local_vars_.clear();
    
    // Process the function body
    if (node->body) {
        std::cout << "Processing function body for: " << func_name << std::endl;
        node->body->accept(this);
    } else {
        std::cout << "No body for function: " << func_name << std::endl;
    }
    
    // Add automatic void return for void functions that don't explicitly return
    if (return_type.kind == IRType::Kind::Void) {
        ir_builder_->ret_void();
    }
    
    // End the function
    ir_builder_->function_end();
}

void CodeGenerator::visit(BlockStatementNode* node) {
    if (!node) return;
    
    // Visit all statements in the block
    for (int i = 0; i < node->statements.size; ++i) {
        node->statements[i]->accept(this);
    }
}

void CodeGenerator::generate_code(CompilationUnitNode* root) {
    if (!root) {
        std::cout << "CodeGenerator: No AST root provided" << std::endl;
        return;
    }
    
    std::cout << "CodeGenerator: Starting code generation..." << std::endl;
    
    // Create the IR builder
    ir_builder_ = std::make_unique<IRBuilder>();
    
    // Visit the compilation unit to generate commands
    root->accept(this);
    
    // Debug: dump the command stream
    std::cout << "\nGenerated command stream:" << std::endl;
    ir_builder_->dump_commands();
    
    // Create command processor and generate LLVM IR
    auto processor = std::make_unique<CommandProcessor>("MyModule");
    processor->process(ir_builder_->commands());
    
    // Verify and dump the module
    if (processor->verify_module()) {
        std::cout << "\nGenerated LLVM IR:" << std::endl;
        processor->dump_module();
    } else {
        std::cerr << "Module verification failed!" << std::endl;
    }
    
    std::cout << "CodeGenerator: Code generation complete" << std::endl;
}

} // namespace Mycelium::Scripting::Lang