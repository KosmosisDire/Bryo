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
    MissingSyntax* errorExpr(const std::string& msg);
    MissingSyntax* errorStmt(const std::string& msg);
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
    BaseStmtSyntax* parseTopLevelStatement();

    // ================== Declarations ==================
    bool checkDeclarationStart();
    BaseDeclSyntax* parseDeclaration();
    ModifierKindFlags parseModifiers();
    TypeDecl* parseTypeDecl(ModifierKindFlags modifiers, const Token& startToken);
    EnumCaseDecl* parseEnumCase();
    FunctionDecl* parseFunctionDecl(ModifierKindFlags modifiers, const Token& startToken);
    ConstructorDecl* parseConstructorDecl(ModifierKindFlags modifiers, const Token& startToken);
    BaseDeclSyntax* parseVarDeclaration(ModifierKindFlags modifiers, const Token& startToken);
    BaseExprSyntax* convertToArrayTypeIfNeeded(BaseExprSyntax* expr);
    std::vector<BaseDeclSyntax*> parseTypedMemberDeclarations(ModifierKindFlags modifiers, BaseExprSyntax* type, const Token& startToken);
    void parsePropertyAccessors(PropertyDecl* prop);
    NamespaceDecl* parseNamespaceDecl(const Token& startToken);

    // ================== Statements ==================
    BaseStmtSyntax* parseStatement();
    Block* parseBlock();
    BaseStmtSyntax* parseIfStatement();
    WhileStmt* parseWhileStatement();
    BaseStmtSyntax* parseForStatement();
    ForStmt* parseTraditionalForStatement();
    ReturnStmt* parseReturnStatement();
    BreakStmt* parseBreakStatement();
    ContinueStmt* parseContinueStatement();
    ExpressionStmt* parseExpressionStatement();
    UsingDirective* parseUsingDirective();

    // ================== Expressions (Precedence Climbing) ==================
    BaseExprSyntax* parseExpression(int minPrecedence = 0);
    BaseExprSyntax* parseBinaryExpression(BaseExprSyntax* left, int minPrecedence);
    BaseExprSyntax* parsePrimaryExpression();
    BaseExprSyntax* parsePostfixExpression(BaseExprSyntax* expr);
    BaseExprSyntax* parseUnaryExpression();
    LiteralExpr* parseLiteral();
    BaseExprSyntax* parseNameExpression();
    List<BaseExprSyntax *> parseGenericArgs();
    BaseExprSyntax* parseTypeExpression();
    BaseExprSyntax* parseParenthesizedOrLambda();
    BaseExprSyntax* parseCastExpression();
    BaseExprSyntax* parseArrayLiteral();
    BaseExprSyntax* parseNewExpression();
    BaseExprSyntax* parseLambdaExpression();
    BaseExprSyntax* parseTypeOfExpression();
    BaseExprSyntax* parseSizeOfExpression();
    
    IdentifierNameSyntax* parseIdentifier();
    TypedIdentifier* parseTypedIdentifier();
    List<ParameterDecl*> parseParameterList();
    List<TypeParameterDecl*> parseTypeParameterList();
    List<BaseExprSyntax*> parseBaseTypeList();  // Uses parseExpression() internally
};

} // namespace Bryo