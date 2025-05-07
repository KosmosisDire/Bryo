#pragma once

#include "token.hpp"
#include "ast.hpp"  
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

        std::unique_ptr<ProgramNode> parseProgram();

    private:
        std::unique_ptr<AstNode> parseDefinition();
        std::unique_ptr<BlockNode> parseBlock();
        std::unique_ptr<AstNode> parseStatement();     
        std::unique_ptr<PropertyNode> parseProperty(); 
        std::unique_ptr<ValueNode> parseValue();       

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
