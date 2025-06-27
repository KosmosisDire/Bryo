#pragma once
#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include <string>
#include <cstring>

namespace Mycelium::Testing {

using namespace Mycelium::Scripting::Lang;

class TestASTBuilder {
private:
    AstAllocator& allocator_;
    
public:
    TestASTBuilder(AstAllocator& allocator) : allocator_(allocator) {}
    
    // Helper to create string storage
    std::string_view create_string(const std::string& str) {
        char* buffer = (char*)allocator_.alloc_bytes(str.length() + 1, 1);
        memcpy(buffer, str.c_str(), str.length() + 1);
        return std::string_view(buffer, str.length());
    }
    
    // Create basic nodes
    TokenNode* create_token(TokenKind kind, const std::string& text) {
        auto token = allocator_.alloc<TokenNode>();
        token->tokenKind = kind;
        token->text = create_string(text);
        return token;
    }
    
    IdentifierNode* create_identifier(const std::string& name) {
        auto ident = allocator_.alloc<IdentifierNode>();
        ident->name = create_string(name);
        return ident;
    }
    
    TypeNameNode* create_type_name(const std::string& name) {
        auto type_node = allocator_.alloc<TypeNameNode>();
        type_node->identifier = create_identifier(name);
        return type_node;
    }
    
    // Create expression nodes
    LiteralExpressionNode* create_int_literal(int32_t value) {
        auto literal = allocator_.alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::Integer;
        literal->token = create_token(TokenKind::IntegerLiteral, std::to_string(value));
        return literal;
    }
    
    LiteralExpressionNode* create_bool_literal(bool value) {
        auto literal = allocator_.alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::Boolean;
        literal->token = create_token(TokenKind::True, value ? "true" : "false");
        return literal;
    }
    
    IdentifierExpressionNode* create_identifier_expression(const std::string& name) {
        auto expr = allocator_.alloc<IdentifierExpressionNode>();
        expr->identifier = create_identifier(name);
        return expr;
    }
    
    BinaryExpressionNode* create_binary_expression(ExpressionNode* left, 
                                                   BinaryOperatorKind op, 
                                                   ExpressionNode* right) {
        auto binary = allocator_.alloc<BinaryExpressionNode>();
        binary->left = left;
        binary->opKind = op;
        binary->right = right;
        return binary;
    }
    
    // Create statement nodes
    ReturnStatementNode* create_return_statement(ExpressionNode* expr = nullptr) {
        auto ret = allocator_.alloc<ReturnStatementNode>();
        ret->expression = expr;
        return ret;
    }
    
    BlockStatementNode* create_block_statement(const std::vector<StatementNode*>& statements = {}) {
        auto block = allocator_.alloc<BlockStatementNode>();
        
        if (!statements.empty()) {
            block->statements.size = (int)statements.size();
            block->statements.values = (StatementNode**)allocator_.alloc_bytes(
                sizeof(StatementNode*) * statements.size(), alignof(StatementNode*));
            memcpy(block->statements.values, statements.data(), 
                   sizeof(StatementNode*) * statements.size());
        } else {
            block->statements.size = 0;
            block->statements.values = nullptr;
        }
        
        return block;
    }
    
    FunctionDeclarationNode* create_simple_function(const std::string& name,
                                                    const std::string& return_type = "void",
                                                    BlockStatementNode* body = nullptr) {
        auto func = allocator_.alloc<FunctionDeclarationNode>();
        func->name = create_identifier(name);
        func->returnType = create_type_name(return_type);
        func->body = body;
        
        // Empty parameters for now
        func->parameters.size = 0;
        func->parameters.values = nullptr;
        
        return func;
    }
    
    CompilationUnitNode* create_compilation_unit(const std::vector<StatementNode*>& statements) {
        auto unit = allocator_.alloc<CompilationUnitNode>();
        
        if (!statements.empty()) {
            unit->statements.size = (int)statements.size();
            unit->statements.values = (StatementNode**)allocator_.alloc_bytes(
                sizeof(StatementNode*) * statements.size(), alignof(StatementNode*));
            memcpy(unit->statements.values, statements.data(), 
                   sizeof(StatementNode*) * statements.size());
        } else {
            unit->statements.size = 0;
            unit->statements.values = nullptr;
        }
        
        return unit;
    }
};

} // namespace Mycelium::Testing