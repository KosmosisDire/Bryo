#include "parser.hpp"
#include <iostream>

namespace Mycelium::UI::Lang
{

    Parser::Parser(const std::vector<Token> &tokens) : m_tokens(tokens), m_currentTokenIndex(0) {}

    // --- Entry Point ---
    std::unique_ptr<ProgramNode> Parser::parseProgram()
    {
        auto program = std::make_unique<ProgramNode>();
        while (!isAtEnd())
        {
            try
            {
                program->definitions.push_back(parseDefinition());
            }
            catch (const std::runtime_error &e)
            {
                std::cerr << "Parser Error: " << e.what() << std::endl;
                throw;
            }
        }
        return program;
    }

    // --- Grammar Rule Parsers ---
    std::unique_ptr<AstNode> Parser::parseDefinition()
    {
        if (check(TokenType::IDENTIFIER))
        {
            return parseBlock();
        }
        else
        {
            throw std::runtime_error("Expected a definition (e.g., 'BlockName {') but found " + tokenTypeToString(currentToken().type) + " ('" + currentToken().text + "')");
        }
    }

    std::unique_ptr<BlockNode> Parser::parseBlock()
    {
        Token typeIdToken = consume(TokenType::IDENTIFIER, "Expected block type identifier (e.g., 'Box')");
        auto blockNode = std::make_unique<BlockNode>(typeIdToken.text);

        if (match(TokenType::LPAREN))
        {
            blockNode->args.push_back(parseValue());
            while (!check(TokenType::RPAREN) && !isAtEnd())
            {
                consume(TokenType::COMMA, "Expected ',' between arguments for block '" + typeIdToken.text + "'");
                blockNode->args.push_back(parseValue());
            }
            
            consume(TokenType::RPAREN, "Expected ')' after arguments for block '" + typeIdToken.text + "'");
        }

        consume(TokenType::LBRACE, "Expected '{' to start block body for '" + typeIdToken.text + "'");

        while (!check(TokenType::RBRACE) && !isAtEnd())
        {
            blockNode->statements.push_back(parseStatement());
        }

        consume(TokenType::RBRACE, "Expected '}' to close block body for '" + typeIdToken.text + "'");

        return blockNode;
    }

    std::unique_ptr<AstNode> Parser::parseStatement()
    {
        if (check(TokenType::IDENTIFIER))
        {
            Token next = peekToken();
            if (next.type == TokenType::COLON)
            {
                return parseProperty();
            }
            else if (next.type == TokenType::LPAREN || next.type == TokenType::LBRACE || check(TokenType::LBRACE))
            { 
                if (check(TokenType::IDENTIFIER) && (peekToken().type == TokenType::LBRACE || (peekToken().type == TokenType::LPAREN)))
                {
                    return parseBlock();
                }
            }

            if (peekToken().type == TokenType::LBRACE ||
                (peekToken().type == TokenType::LPAREN && peekToken(1).type == TokenType::STRING_LITERAL && peekToken(2).type == TokenType::RPAREN && peekToken(3).type == TokenType::LBRACE) || // IDENTIFIER ( "string" ) {
                (peekToken().type == TokenType::LPAREN && peekToken(1).type == TokenType::RPAREN && peekToken(2).type == TokenType::LBRACE)                                                      // IDENTIFIER () { -- if you allow this for components later
            )
            {
                return parseBlock();
            }
        }

        throw std::runtime_error("Expected a statement (e.g., nested block, property) inside block, but found " + tokenTypeToString(currentToken().type) + " ('" + currentToken().text + "')");
    }

    std::unique_ptr<PropertyNode> Parser::parseProperty()
    {
        Token nameToken = consume(TokenType::IDENTIFIER, "Expected property name.");
        consume(TokenType::COLON, "Expected ':' after property name '" + nameToken.text + "'.");
        std::unique_ptr<ValueNode> valNode = parseValue();
        consume(TokenType::SEMICOLON, "Expected ';' after property value for '" + nameToken.text + "'.");
        return std::make_unique<PropertyNode>(nameToken.text, std::move(valNode));
    }

    std::unique_ptr<ValueNode> Parser::parseValue()
    {
        if (check(TokenType::NUMBER))
        {
            Token numToken = consume(TokenType::NUMBER, "Expected number.");
            double numValue = std::stod(numToken.text);
            bool isPercent = false;
            if (match(TokenType::PERCENTAGE_SIGN))
            {
                isPercent = true;
            }
            return std::make_unique<NumberLiteralNode>(numValue, isPercent);
        }
        else if (check(TokenType::STRING_LITERAL))
        {
            Token strToken = consume(TokenType::STRING_LITERAL, "Expected string literal value.");
            return std::make_unique<StringLiteralValueNode>(strToken.text);
        }

        throw std::runtime_error("Expected a value (number, string, etc.) but found " + tokenTypeToString(currentToken().type) + " ('" + currentToken().text + "')");
    }

    // --- Parser Helper Methods ---
    const Token &Parser::currentToken() const
    {
        if (m_currentTokenIndex < m_tokens.size())
        {
            return m_tokens[m_currentTokenIndex];
        }
        // Return reference to a static EOF token to avoid issues if called when at end
        static Token eof_token = {TokenType::END_OF_FILE, "", 0, 0};
        return eof_token;
    }

    const Token &Parser::peekToken(size_t lookahead) const
    {
        size_t index = m_currentTokenIndex + lookahead;
        if (index < m_tokens.size())
        {
            return m_tokens[index];
        }
        static Token eof_token = {TokenType::END_OF_FILE, "", 0, 0};
        return eof_token;
    }

    bool Parser::isAtEnd() const
    {
        return currentToken().type == TokenType::END_OF_FILE;
    }

    bool Parser::check(TokenType type) const
    {
        if (isAtEnd())
            return false;
        return currentToken().type == type;
    }

    bool Parser::match(TokenType type)
    {
        if (check(type))
        {
            advance();
            return true;
        }
        return false;
    }

    Token Parser::consume(TokenType expectedType, const std::string &errorMessage)
    {
        const Token &token = currentToken();
        if (token.type == expectedType)
        {
            advance();
            return m_tokens[m_currentTokenIndex - 1];
        }
        else
        {
            std::string error = errorMessage;
            error += " Found " + tokenTypeToString(token.type);
            error += " ('" + token.text + "') instead.";

            throw std::runtime_error(error);
        }
    }

    Token Parser::consume()
    {
        if (isAtEnd())
        {
            throw std::runtime_error("Attempted to consume token past end of file.");
        }
        Token current = currentToken();
        advance();
        return current;
    }

    Token Parser::consumeAny(std::vector<TokenType> expectedTypes, const std::string &errorMessage)
    {
        const Token &token = currentToken();
        for (const auto &type : expectedTypes)
        {
            if (token.type == type)
            {
                advance();
                return m_tokens[m_currentTokenIndex - 1];
            }
        }

        std::string error = errorMessage;
        error += " Found " + tokenTypeToString(token.type);
        error += " ('" + token.text + "') instead.";

        throw std::runtime_error(error);
    }

    void Parser::advance()
    {

        if (m_currentTokenIndex < m_tokens.size() && m_tokens[m_currentTokenIndex].type != TokenType::END_OF_FILE)
        {
            m_currentTokenIndex++;
        }
    }

}