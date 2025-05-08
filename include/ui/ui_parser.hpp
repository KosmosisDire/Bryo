#pragma once

#include "ui_token.hpp"
#include "ui_ast.hpp"  
#include <vector>
#include <memory>   
#include <string>   
#include <stdexcept>

namespace Mycelium::UI::Lang
{
    struct ProgramNode;
    struct AstNode;
    struct BlockNode;

    class Parser
    {
    public:
        Parser(const std::vector<Token> &tokens);

        std::shared_ptr<ProgramNode> parseProgram();

    private:
        std::shared_ptr<AstNode> parseDefinition();
        std::shared_ptr<BlockNode> parseBlock(std::shared_ptr<BlockNode> parent);
        std::shared_ptr<AstNode> parseStatement(std::shared_ptr<BlockNode> parent);    
        std::shared_ptr<PropertyAssignmentNode> parseProperty(std::shared_ptr<BlockNode> parent);
        std::shared_ptr<ValueNode> parseValue();

        const Token &currentToken() const;
        const Token &peekToken(size_t lookahead = 1) const;
        bool isAtEnd() const;
        Token consume(TokenType expectedType, const std::string &errorMessage);
        Token consume();
        Token consumeAny(std::vector<TokenType> expectedTypes, const std::string &errorMessage);
        void advance();
        bool check(TokenType type) const;
        bool match(TokenType type);      

        const std::vector<Token> &m_tokens;
        size_t m_currentTokenIndex;
    };

}
