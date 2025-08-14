#pragma once

#include "ast/ast.hpp"
#include "ast/arena.hpp"
#include "token_stream.hpp"
#include <vector>
#include <optional>
#include <string>
#include <initializer_list>
#include <functional> // For std::invoke_result_t

namespace Myre {

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
    ErrorTypeRef* errorType(const std::string& msg);
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

    // ================== Top Level Parsing ==================
    Statement* parseTopLevelStatement();

    // ================== Declarations ==================
    bool checkDeclarationStart();
    Declaration* parseDeclaration();
    ModifierSet parseModifiers();
    TypeDecl* parseTypeDecl(ModifierSet modifiers);
    EnumCaseDecl* parseEnumCase();
    FunctionDecl* parseFunctionDecl(ModifierSet modifiers);
    ConstructorDecl* parseConstructorDecl(ModifierSet modifiers);
    InheritFunctionDecl* parseInheritFunctionDecl(ModifierSet modifiers);
    Declaration* parseVarDeclaration(ModifierSet modifiers);
    Declaration* parseTypedMemberDeclaration(ModifierSet modifiers, TypeRef* type);
    MemberVariableDecl* parseMemberVariableDecl(ModifierSet modifiers, TypeRef* type, bool isVar);
    void parsePropertyAccessors(MemberVariableDecl* prop);
    NamespaceDecl* parseNamespaceDecl();

    // ================== Statements ==================
    Statement* parseStatement();
    Block* parseBlock();
    Statement* parseIfStatement();
    WhileStmt* parseWhileStatement();
    Statement* parseForStatement();
    ForStmt* parseTraditionalForStatement();
    ForInStmt* parseForInStatement();
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
    Expression* parseRangeExpression(Expression* start);
    Expression* parseLiteral();
    Expression* parseNameExpression();
    Expression* parseParenthesizedOrLambda();
    Expression* parseArrayLiteral();
    Expression* parseCallExpression(Expression* callee);
    Expression* parseNewExpression();
    Expression* parseLambdaExpression();
    Expression* parseMatchExpression();
    MatchArm* parseMatchArm();
    Expression* parseTypeOfExpression();
    Expression* parseSizeOfExpression();

    // ================== Types ==================
    TypeRef* parseTypeRef();
    NamedTypeRef* parseNamedType();
    FunctionTypeRef* parseFunctionType();

    // ================== Patterns ==================
    Pattern* parsePattern();
    Pattern* parseRangePattern();
    Pattern* parseEnumPattern();
    Pattern* parseComparisonPattern();

    // ================== Helper Functions ==================
    Identifier* parseIdentifier();
    TypedIdentifier* parseTypedIdentifier();
    List<GenericParamDecl*> parseGenericParams();
    List<TypeRef*> parseGenericArguments();
    List<ParameterDecl*> parseParameterList();
    List<TypeRef*> parseBaseTypeList();
    void parseWhereConstraints(List<GenericParamDecl*> params);
};

} // namespace Myre