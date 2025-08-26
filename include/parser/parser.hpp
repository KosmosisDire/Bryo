#pragma once

#include "ast/ast.hpp"
#include "ast/arena.hpp"
#include "token_stream.hpp"
#include "common/token.hpp"
#include <vector>
#include <optional>
#include <string>
#include <initializer_list>
#include <functional> // For std::invoke_result_t

namespace Bryo {

class Parser {
public:
    Parser(TokenStream& tokens);
    ~Parser();

    // Main entry point
    CompilationUnit* parse();

    // Error tracking
    struct ParseError {
        std::string message;
        SourceRange location;
        enum Level { WARNING, ERROR, FATAL } level;
    };

    const std::vector<ParseError>& getErrors() const;
    bool hasErrors() const;

private:
    // Data Members
    Arena arena;
    TokenStream& tokens;
    std::vector<ParseError> errors;

    // Context tracking
    enum class Context {
        TOP_LEVEL,
        TYPE_BODY,
        NAMESPACE,
        FUNCTION,
        LOOP,
        PROPERTY_GETTER,
        PROPERTY_SETTER
    };
    std::vector<Context> contextStack;

    // ================== Error Handling ==================
    void error(const std::string& msg);
    void warning(const std::string& msg);
    ErrorExpression* errorExpr(const std::string& msg);
    ErrorStatement* errorStmt(const std::string& msg);
    void synchronize();

    // ================== Context Management ==================
    bool inLoop() const;
    bool inFunction() const;
    bool inGetter() const;
    bool inSetter() const;
    bool inTypeBody() const;

    // Template function must be defined in the header.
    template <typename F>
    auto withContext(Context type, F &&func)
    {
        contextStack.push_back(type);
        if constexpr (std::is_void_v<std::invoke_result_t<F>>)
        {
            func();
            contextStack.pop_back();
        }
        else
        {
            auto result = func();
            contextStack.pop_back();
            return result;
        }
    }

    // ================== Utility Helpers ==================
    bool check(TokenKind kind);
    bool checkAny(std::initializer_list<TokenKind> kinds);
    bool consume(TokenKind kind);
    bool expect(TokenKind kind, const std::string& msg);
    Token previous();
    TokenKind peekNext();
    bool isExpressionTerminator();
    bool isPatternTerminator();
    bool isOnSameLine(const Token& prev, const Token& curr) const;
    bool requireSemicolonIfSameLine();

    // ================== Top Level Parsing ==================
    Statement* parseTopLevelStatement();

    // ================== Declarations ==================
    bool checkDeclarationStart();
    Declaration* parseDeclaration();
    ModifierKindFlags parseModifiers();
    TypeDecl* parseTypeDecl(ModifierKindFlags modifiers, const Token& startToken);
    EnumCaseDecl* parseEnumCase();
    FunctionDecl* parseFunctionDecl(ModifierKindFlags modifiers, const Token& startToken);
    ConstructorDecl* parseConstructorDecl(ModifierKindFlags modifiers, const Token& startToken);
    Declaration* parseVarDeclaration(ModifierKindFlags modifiers, const Token& startToken);
    Expression* convertToArrayTypeIfNeeded(Expression* expr);
    std::vector<Declaration*> parseTypedMemberDeclarations(ModifierKindFlags modifiers, Expression* type, const Token& startToken);
    void parsePropertyAccessors(PropertyDecl* prop);
    NamespaceDecl* parseNamespaceDecl(const Token& startToken);

    // ================== Statements ==================
    Statement* parseStatement();
    Block* parseBlock();
    Statement* parseIfStatement();
    WhileStmt* parseWhileStatement();
    Statement* parseForStatement();
    ForStmt* parseTraditionalForStatement();
    ReturnStmt* parseReturnStatement();
    BreakStmt* parseBreakStatement();
    ContinueStmt* parseContinueStatement();
    ExpressionStmt* parseExpressionStatement();
    UsingDirective* parseUsingDirective();

    // ================== Expressions (Precedence Climbing) ==================
    Expression* parseExpression(int minPrecedence = 0);
    Expression* parseBinaryExpression(Expression* left, int minPrecedence);
    Expression* parsePrimaryExpression();
    Expression* parsePostfixExpression(Expression* expr);
    Expression* parseUnaryExpression();
    LiteralExpr* parseLiteral();
    Expression* parseNameExpression();
    List<Expression *> parseGenericArgs();
    Expression* parseTypeExpression();
    Expression* parseParenthesizedOrLambda();
    Expression* parseCastExpression();
    Expression* parseArrayLiteral();
    Expression* parseNewExpression();
    Expression* parseLambdaExpression();
    Expression* parseTypeOfExpression();
    Expression* parseSizeOfExpression();
    
    Identifier* parseIdentifier();
    TypedIdentifier* parseTypedIdentifier();
    List<ParameterDecl*> parseParameterList();
    List<TypeParameterDecl*> parseTypeParameterList();
    List<Expression*> parseBaseTypeList();  // Uses parseExpression() internally
};

} // namespace Bryo