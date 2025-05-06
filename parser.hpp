#pragma once

#include "token.hpp" // Needs Token, TokenType
#include "ast.hpp"   // Needs AST node types (forward declared is ok)
#include <vector>
#include <memory>    // For unique_ptr
#include <string>    // For error messages
#include <stdexcept> // For runtime_error

namespace Mycelium::UI::Lang
{

    // Forward declare AST node types used as return types
    struct ProgramNode;
    struct AstNode;
    struct BlockNode;

    class Parser
    {
    public:
        Parser(const std::vector<Token> &tokens);

        std::unique_ptr<ProgramNode> parseProgram(); // Entry point

    private:
        // --- Recursive Descent Methods (one per grammar rule) ---
        std::unique_ptr<AstNode> parseDefinition(); // Dispatches to Block, etc.
        std::unique_ptr<BlockNode> parseBlock();
        std::unique_ptr<AstNode> parseStatement();     // Dispatches to Block, Property, etc.
        std::unique_ptr<PropertyNode> parseProperty(); // New method for properties
        std::unique_ptr<ValueNode> parseValue();       // New method for values (numbers, strings, etc.)

        // --- Parser Helper Methods ---
        const Token &currentToken() const;
        const Token &peekToken(size_t lookahead = 1) const; // Look ahead
        bool isAtEnd() const;
        Token consume(TokenType expectedType, const std::string &errorMessage);
        Token consume(); // Consume whatever token is current
        void advance();
        bool check(TokenType type) const; // Check current token type without consuming
        bool match(TokenType type);       // Consume if matches, return true/false

        const std::vector<Token> &m_tokens;
        size_t m_currentTokenIndex;
    };

} // namespace Mycelium::UI::Lang
