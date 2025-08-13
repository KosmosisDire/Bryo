#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <span>
#include <optional>
#include <vector>
#include "common/symbol_handle.hpp"
#include "common/source_location.hpp"
#include "common/token.hpp"

namespace Myre {

// ============================================================================
// --- Forward Declarations ---
// ============================================================================
struct Token;

// ============================================================================
// --- Forward Declarations ---
// ============================================================================
struct Token;

struct Node;
struct Expression;
struct Statement;
struct Declaration;
struct TypeRef;
struct Pattern;
struct TypeConstraint;
struct PropertyAccessor;
struct ParameterDecl;
class Visitor;

// Add all concrete node forward declarations
struct Identifier;
struct TypedIdentifier;
struct ErrorExpression;
struct ErrorStatement;
struct ErrorTypeRef;

// Expressions
struct LiteralExpr;
struct ArrayLiteralExpr;
struct NameExpr;
struct UnaryExpr;
struct BinaryExpr;
struct AssignmentExpr;
struct CallExpr;
struct MemberAccessExpr;
struct IndexerExpr;
struct CastExpr;
struct NewExpr;
struct ThisExpr;
struct LambdaExpr;
struct RangeExpr;
struct ConditionalExpr;
struct TypeOfExpr;
struct SizeOfExpr;
struct Block;
struct IfExpr;
struct MatchExpr;
struct MatchArm;

// Statements
struct ExpressionStmt;
struct ReturnStmt;
struct BreakStmt;
struct ContinueStmt;
struct WhileStmt;
struct ForStmt;
struct ForInStmt;
struct UsingDirective;

// Declarations
struct VariableDecl;
struct MemberVariableDecl;
struct GenericParamDecl;
struct FunctionDecl;
struct ConstructorDecl;
struct InheritFunctionDecl;
struct EnumCaseDecl;
struct TypeDecl;
struct NamespaceDecl;

// Type references
struct NamedTypeRef;
struct ArrayTypeRef;
struct FunctionTypeRef;
struct NullableTypeRef;
struct RefTypeRef;

// Type constraints
struct BaseTypeConstraint;
struct ConstructorConstraint;
struct TypeKindConstraint;

// Patterns
struct LiteralPattern;
struct BindingPattern;
struct EnumPattern;
struct RangePattern;
struct InPattern;
struct ComparisonPattern;

// Root
struct CompilationUnit;

// Non-owning collection type (arena allocated)
template<typename T>
using List = std::span<T>;

// Macro to create an accept function implementation
#define ACCEPT_VISITOR void accept(Visitor* visitor) override { visitor->visit(this); }


// ============================================================================
// --- Visitor Pattern ---
// ============================================================================

class Visitor {
public:
    virtual ~Visitor() = default;
    
    // Base type visits - can be overridden for uniform handling
    virtual void visit(Node* node);
    virtual void visit(Expression* node);
    virtual void visit(Statement* node);
    virtual void visit(Declaration* node);
    virtual void visit(Pattern* node);
    virtual void visit(TypeRef* node);
    virtual void visit(TypeConstraint* node);
    
    // All concrete node types
    virtual void visit(Identifier* node) = 0;
    virtual void visit(TypedIdentifier* node) = 0;
    virtual void visit(ErrorExpression* node) = 0;
    virtual void visit(ErrorStatement* node) = 0;
    virtual void visit(ErrorTypeRef* node) = 0;
    
    // Expressions
    virtual void visit(LiteralExpr* node) = 0;
    virtual void visit(ArrayLiteralExpr* node) = 0;
    virtual void visit(NameExpr* node) = 0;
    virtual void visit(UnaryExpr* node) = 0;
    virtual void visit(BinaryExpr* node) = 0;
    virtual void visit(AssignmentExpr* node) = 0;
    virtual void visit(CallExpr* node) = 0;
    virtual void visit(MemberAccessExpr* node) = 0;
    virtual void visit(IndexerExpr* node) = 0;
    virtual void visit(CastExpr* node) = 0;
    virtual void visit(NewExpr* node) = 0;
    virtual void visit(ThisExpr* node) = 0;
    virtual void visit(LambdaExpr* node) = 0;
    virtual void visit(RangeExpr* node) = 0;
    virtual void visit(ConditionalExpr* node) = 0;
    virtual void visit(TypeOfExpr* node) = 0;
    virtual void visit(SizeOfExpr* node) = 0;
    virtual void visit(Block* node) = 0;
    virtual void visit(IfExpr* node) = 0;
    virtual void visit(MatchExpr* node) = 0;
    virtual void visit(MatchArm* node) = 0;
    
    // Statements
    virtual void visit(ExpressionStmt* node) = 0;
    virtual void visit(ReturnStmt* node) = 0;
    virtual void visit(BreakStmt* node) = 0;
    virtual void visit(ContinueStmt* node) = 0;
    virtual void visit(WhileStmt* node) = 0;
    virtual void visit(ForStmt* node) = 0;
    virtual void visit(ForInStmt* node) = 0;
    virtual void visit(UsingDirective* node) = 0;
    
    // Declarations
    virtual void visit(VariableDecl* node) = 0;
    virtual void visit(MemberVariableDecl* node) = 0;
    virtual void visit(ParameterDecl* node) = 0;
    virtual void visit(GenericParamDecl* node) = 0;
    virtual void visit(FunctionDecl* node) = 0;
    virtual void visit(ConstructorDecl* node) = 0;
    virtual void visit(PropertyAccessor* node) = 0;
    virtual void visit(InheritFunctionDecl* node) = 0;
    virtual void visit(EnumCaseDecl* node) = 0;
    virtual void visit(TypeDecl* node) = 0;
    virtual void visit(NamespaceDecl* node) = 0;
    
    // Type references
    virtual void visit(NamedTypeRef* node) = 0;
    virtual void visit(ArrayTypeRef* node) = 0;
    virtual void visit(FunctionTypeRef* node) = 0;
    virtual void visit(NullableTypeRef* node) = 0;
    virtual void visit(RefTypeRef* node) = 0;
    
    // Type constraints
    virtual void visit(BaseTypeConstraint* node) = 0;
    virtual void visit(ConstructorConstraint* node) = 0;
    virtual void visit(TypeKindConstraint* node) = 0;

    // Patterns
    virtual void visit(LiteralPattern* node) = 0;
    virtual void visit(BindingPattern* node) = 0;
    virtual void visit(EnumPattern* node) = 0;
    virtual void visit(RangePattern* node) = 0;
    virtual void visit(InPattern* node) = 0;
    virtual void visit(ComparisonPattern* node) = 0;
    
    // Root
    virtual void visit(CompilationUnit* node) = 0;
};


// ============================================================================
// --- Modifiers ---
// ============================================================================

struct ModifierSet {
    enum class Access { None, Public, Protected, Private, Internal };
    
    Access access = Access::None;
    bool isStatic = false;
    bool isVirtual = false;
    bool isAbstract = false;
    bool isOverride = false;
    bool isRef = false;
    bool isEnforced = false;
    bool isInherit = false;
    bool isReadonly = false;
};

// ============================================================================
// --- Core Node Hierarchy ---
// ============================================================================

struct Node {
    SourceRange location;
    SymbolHandle resolvedSymbol = {0};
    
    virtual ~Node() = default;
    virtual void accept(Visitor* visitor);
    
    // Clean API using dynamic_cast
    template<typename T>
    bool is() const {
        return dynamic_cast<const T*>(this) != nullptr;
    }
    
    template<typename T>
    T* as() {
        return dynamic_cast<T*>(this);
    }
    
    template<typename T>
    const T* as() const {
        return dynamic_cast<const T*>(this);
    }
};

struct Expression : Node {
    // Cached semantic analysis data
    TypeRef* resolvedType = nullptr;
    bool isLValue = false;
    bool isConstant = false;
    ACCEPT_VISITOR
};

struct Statement : Node {
    ACCEPT_VISITOR
};

// Base for declarations - no name field!
struct Declaration : Statement {
    ModifierSet modifiers;
    ACCEPT_VISITOR
};

struct Pattern : Node {
    ACCEPT_VISITOR
};

struct TypeRef : Node {
    ACCEPT_VISITOR
};

struct TypeConstraint : Node {
    ACCEPT_VISITOR
};

// ============================================================================
// --- Basic Building Blocks ---
// ============================================================================

struct Identifier : Node {
    std::string_view text;
    ACCEPT_VISITOR
};

// Reusable component for name + optional type
struct TypedIdentifier : Node {
    Identifier* name;
    TypeRef* type;  // null = inferred (var)
    ACCEPT_VISITOR
};

// ============================================================================
// --- Error Nodes (for robust error recovery) ---
// ============================================================================

struct ErrorExpression : Expression {
    std::string_view message;
    List<Node*> partialNodes;
    ACCEPT_VISITOR
};

struct ErrorStatement : Statement {
    std::string_view message;
    List<Node*> partialNodes;
    ACCEPT_VISITOR
};

struct ErrorTypeRef : TypeRef {
    std::string_view message;
    ACCEPT_VISITOR
};

// ============================================================================
// --- Expressions ---
// ============================================================================

struct LiteralExpr : Expression {
    enum class Kind {
        Integer, Float, String, Char, Bool, Null
    };
    
    Kind kind;
    std::string_view value;  // Raw text from source
    ACCEPT_VISITOR
};

struct ArrayLiteralExpr : Expression {
    List<Expression*> elements;
    ACCEPT_VISITOR
};

struct NameExpr : Expression {
    List<Identifier*> parts;  // ["Console", "WriteLine"]
    ACCEPT_VISITOR
};

struct UnaryExpr : Expression {
    UnaryOperatorKind op;
    Expression* operand;  // Never null (ErrorExpression if parse fails)
    bool isPostfix;
    ACCEPT_VISITOR
};

struct BinaryExpr : Expression {
    Expression* left;   // Never null
    BinaryOperatorKind op;
    Expression* right;  // Never null
    ACCEPT_VISITOR
};

struct AssignmentExpr : Expression {
    Expression* target;  // Never null, must be lvalue
    AssignmentOperatorKind op;
    Expression* value;   // Never null
    ACCEPT_VISITOR
};

struct CallExpr : Expression {
    Expression* callee;  // Never null
    List<Expression*> arguments;
    ACCEPT_VISITOR
};

struct MemberAccessExpr : Expression {
    Expression* object;  // Never null
    Identifier* member;  // Never null
    ACCEPT_VISITOR
};

struct IndexerExpr : Expression {
    Expression* object;  // Never null
    Expression* index;   // Never null (can be RangeExpr for slicing)
    ACCEPT_VISITOR
};

struct CastExpr : Expression {
    TypeRef* targetType;  // Never null
    Expression* expression;  // Never null
    ACCEPT_VISITOR
};

struct NewExpr : Expression {
    TypeRef* type;  // Never null
    List<Expression*> arguments;
    ACCEPT_VISITOR
};

struct ThisExpr : Expression {
    ACCEPT_VISITOR
};

struct LambdaExpr : Expression {
    List<ParameterDecl*> parameters;
    Statement* body;  // Block or ExpressionStmt
    ACCEPT_VISITOR
};

struct RangeExpr : Expression {
    Expression* start;      // Can be null (open start)
    Expression* end;        // Can be null (open end)
    Expression* step;       // Can be null (default step)
    bool isInclusive;       // .. vs ..=
    ACCEPT_VISITOR
};

struct ConditionalExpr : Expression {
    Expression* condition;  // Never null
    Expression* thenExpr;   // Never null
    Expression* elseExpr;   // Never null
    ACCEPT_VISITOR
};

struct TypeOfExpr : Expression {
    TypeRef* type;  // Never null
    ACCEPT_VISITOR
};

struct SizeOfExpr : Expression {
    TypeRef* type;  // Never null
    ACCEPT_VISITOR
};

// Single block type for both statements and expressions
struct Block : Statement {
    List<Statement*> statements;
    ACCEPT_VISITOR
};

struct IfExpr : Expression {
    Expression* condition;    // Never null
    Statement* thenBranch;    // Never null (usually Block)
    Statement* elseBranch;    // Can be null
    ACCEPT_VISITOR
};

struct MatchArm : Node {
    Pattern* pattern;         // Never null
    Statement* result;       // Never null
    ACCEPT_VISITOR
};

struct MatchExpr : Expression {
    Expression* subject;      // Never null
    List<MatchArm*> arms;
    ACCEPT_VISITOR
};

// ============================================================================
// --- Statements ---
// ============================================================================

struct ExpressionStmt : Statement {
    Expression* expression;  // Never null
    ACCEPT_VISITOR
};

struct ReturnStmt : Statement {
    Expression* value;  // Can be null (void return)
    ACCEPT_VISITOR
};

struct BreakStmt : Statement {
    ACCEPT_VISITOR
};

struct ContinueStmt : Statement {
    ACCEPT_VISITOR
};

struct WhileStmt : Statement {
    Expression* condition;  // Never null
    Statement* body;        // Never null (usually Block)
    ACCEPT_VISITOR
};

struct ForStmt : Statement {
    Statement* initializer;     // Can be null
    Expression* condition;      // Can be null (infinite loop)
    List<Expression*> updates;
    Statement* body;            // Never null
    ACCEPT_VISITOR
};

struct ForInStmt : Statement {
    TypedIdentifier* iterator;  // Never null
    Expression* iterable;       // Never null
    TypedIdentifier* indexVar;  // Can be null ("at" clause)
    Statement* body;            // Never null
    ACCEPT_VISITOR
};

struct UsingDirective : Statement {
    enum class Kind { 
        Namespace,  // using System.Collections;
        Alias       // using Dict = Dictionary<string, int>;
    };
    
    Kind kind;
    List<Identifier*> path;
    
    // For alias only
    Identifier* alias = nullptr;
    TypeRef* aliasedType = nullptr;
    ACCEPT_VISITOR
};

// ============================================================================
// --- Declarations ---
// ============================================================================

// Regular local variable: var x = 5;
struct VariableDecl : Declaration {
    TypedIdentifier* variable;  // Never null
    Expression* initializer;    // Can be null
    ACCEPT_VISITOR
};

// Unified field/property for class members
struct MemberVariableDecl : Declaration {
    Identifier* name;           // Never null
    TypeRef* type;              // Never null
    Expression* initializer;    // Can be null
    PropertyAccessor* getter;   // null = field, non-null = property
    PropertyAccessor* setter;   // Can be null even for properties
    ACCEPT_VISITOR
};

struct ParameterDecl : Declaration {
    TypedIdentifier* param;     // Never null
    Expression* defaultValue;   // Can be null
    ACCEPT_VISITOR
};

struct GenericParamDecl : Declaration {
    Identifier* name;           // Never null
    List<TypeConstraint*> constraints;
    ACCEPT_VISITOR
};

struct FunctionDecl : Declaration {
    Identifier* name;           // Never null
    List<GenericParamDecl*> genericParams;
    List<ParameterDecl*> parameters;
    TypeRef* returnType;        // null = void
    Block* body;                // Can be null (abstract)
    ACCEPT_VISITOR
};

struct ConstructorDecl : Declaration {
    // No name field - constructors are always "new"
    List<ParameterDecl*> parameters;
    Block* body;                // Never null
    ACCEPT_VISITOR
};

struct PropertyAccessor : Node
{
    enum class Kind { Get, Set };
    Kind kind;
    ModifierSet modifiers;
    
    // Body representation
    std::variant<
        std::monostate,         // Default/auto-implemented
        Expression*,            // Expression-bodied: => expr
        Block*                  // Block-bodied: { ... }
    > body;
    ACCEPT_VISITOR
};

struct InheritFunctionDecl : Declaration {
    // No name field in base - this is about inheriting a function implementation
    Identifier* functionName;   // Never null
    List<TypeRef*> parameterTypes;  // For overload resolution
    ACCEPT_VISITOR
};

struct EnumCaseDecl : Declaration {
    Identifier* name;           // Never null
    List<ParameterDecl*> associatedData;  // Can be empty
    ACCEPT_VISITOR
};

struct TypeDecl : Declaration {
    enum class Kind { 
        Type,       // type
        ValueType,  // value type
        RefType,    // ref type
        StaticType, // static type (all members implicitly static)
        Enum        // enum
    };
    
    Identifier* name;           // Never null
    Kind kind;
    List<GenericParamDecl*> genericParams;
    List<TypeRef*> baseTypes;
    List<Declaration*> members;
    ACCEPT_VISITOR
};

struct NamespaceDecl : Declaration {
    List<Identifier*> path;     // Never empty
    bool isFileScoped;          
    std::optional<List<Statement*>> body;  // nullopt for file-scoped
    ACCEPT_VISITOR
};

// ============================================================================
// --- Type System ---
// ============================================================================

struct NamedTypeRef : TypeRef {
    List<Identifier*> path;     // Never empty
    List<TypeRef*> genericArgs;
    ACCEPT_VISITOR
};

struct ArrayTypeRef : TypeRef {
    TypeRef* elementType;       // Never null
    ACCEPT_VISITOR
};

struct FunctionTypeRef : TypeRef {
    List<TypeRef*> parameterTypes;
    TypeRef* returnType;        // null = void
    ACCEPT_VISITOR
};

struct NullableTypeRef : TypeRef {
    TypeRef* innerType;         // Never null
    ACCEPT_VISITOR
};

struct RefTypeRef : TypeRef {
    TypeRef* innerType;         // Never null
    ACCEPT_VISITOR
};

// ============================================================================
// --- Type Constraints (for generics) ---
// ============================================================================

struct BaseTypeConstraint : TypeConstraint {
    TypeRef* baseType;          // Never null
    ACCEPT_VISITOR
};

struct ConstructorConstraint : TypeConstraint {
    List<TypeRef*> parameterTypes;  // Empty for parameterless
    ACCEPT_VISITOR
};

struct TypeKindConstraint : TypeConstraint {
    enum class Kind { RefType, ValueType, ArrayType, FunctionType };
    Kind kind;
    ACCEPT_VISITOR
};

// ============================================================================
// --- Pattern Matching ---
// ============================================================================

struct LiteralPattern : Pattern {
    LiteralExpr* literal;       // Never null
    ACCEPT_VISITOR
};

struct BindingPattern : Pattern {
    Identifier* name;           // null = wildcard (_)
    ACCEPT_VISITOR
};

struct EnumPattern : Pattern {
    List<Identifier*> path;     // Never empty
    List<Pattern*> argumentPatterns;
    ACCEPT_VISITOR
};

struct RangePattern : Pattern {
    Expression* start;          // Can be null
    Expression* end;            // Can be null
    bool isInclusive;
    ACCEPT_VISITOR
};

struct InPattern : Pattern {
    Pattern* innerPattern;      // Never null
    ACCEPT_VISITOR
};

struct ComparisonPattern : Pattern {
    enum class Op { Less, Greater, LessEqual, GreaterEqual };
    Op op;
    Expression* value;          // Never null
    ACCEPT_VISITOR
};

// ============================================================================
// --- Root Node ---
// ============================================================================

struct CompilationUnit : Node {
    List<Statement*> topLevelStatements;
    ACCEPT_VISITOR
};

// ============================================================================
// --- Default Visitor (with automatic traversal) ---
// ============================================================================

class DefaultVisitor : public Visitor {
public:
    // Base type visit implementations - override these for uniform handling
    void visit(Node* node) override { /* default: do nothing */ }
    void visit(Expression* node) override { visit(static_cast<Node*>(node)); }
    void visit(Statement* node) override { visit(static_cast<Node*>(node)); }
    void visit(Declaration* node) override { visit(static_cast<Statement*>(node)); }
    void visit(Pattern* node) override { visit(static_cast<Node*>(node)); }
    void visit(TypeRef* node) override { visit(static_cast<Node*>(node)); }
    void visit(TypeConstraint* node) override { visit(static_cast<Node*>(node)); }
    
    // Default implementations that traverse children
    void visit(Identifier* node) override { visit(static_cast<Node*>(node)); }

    void visit(TypedIdentifier* node) override {
        visit(static_cast<Node*>(node));
        if (node->name) node->name->accept(this);
        if (node->type) node->type->accept(this);
    }
    
    void visit(ErrorExpression* node) override {
        visit(static_cast<Expression*>(node));
        for (auto* partial : node->partialNodes) {
            if (partial) partial->accept(this);
        }
    }
    
    void visit(ErrorStatement* node) override {
        visit(static_cast<Statement*>(node));
        for (auto* partial : node->partialNodes) {
            if (partial) partial->accept(this);
        }
    }
    
    void visit(ErrorTypeRef* node) override { 
        visit(static_cast<TypeRef*>(node));
    }
    
    void visit(LiteralExpr* node) override { 
        visit(static_cast<Expression*>(node));
    }
    
    void visit(ArrayLiteralExpr* node) override {
        visit(static_cast<Expression*>(node));
        for (auto* elem : node->elements) {
            elem->accept(this);
        }
    }
    
    void visit(NameExpr* node) override {
        visit(static_cast<Expression*>(node));
        for (auto* part : node->parts) {
            part->accept(this);
        }
    }
    
    void visit(UnaryExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->operand->accept(this);
    }
    
    void visit(BinaryExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->left->accept(this);
        node->right->accept(this);
    }
    
    void visit(AssignmentExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->target->accept(this);
        node->value->accept(this);
    }
    
    void visit(CallExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->callee->accept(this);
        for (auto* arg : node->arguments) {
            arg->accept(this);
        }
    }
    
    void visit(MemberAccessExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->object->accept(this);
        node->member->accept(this);
    }
    
    void visit(IndexerExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->object->accept(this);
        node->index->accept(this);
    }
    
    void visit(CastExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->targetType->accept(this);
        node->expression->accept(this);
    }
    
    void visit(NewExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->type->accept(this);
        for (auto* arg : node->arguments) {
            arg->accept(this);
        }
    }
    
    void visit(ThisExpr* node) override { 
        visit(static_cast<Expression*>(node));
    }
    
    void visit(LambdaExpr* node) override {
        visit(static_cast<Expression*>(node));
        for (auto* param : node->parameters) {
            param->accept(this);
        }
        if (node->body) node->body->accept(this);
    }
    
    void visit(RangeExpr* node) override {
        visit(static_cast<Expression*>(node));
        if (node->start) node->start->accept(this);
        if (node->end) node->end->accept(this);
        if (node->step) node->step->accept(this);
    }
    
    void visit(ConditionalExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->condition->accept(this);
        node->thenExpr->accept(this);
        node->elseExpr->accept(this);
    }
    
    void visit(TypeOfExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->type->accept(this);
    }
    
    void visit(SizeOfExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->type->accept(this);
    }

    void visit(Block* node) override {
        visit(static_cast<Statement*>(node));
        for (auto* stmt : node->statements) {
            stmt->accept(this);
        }
    }
    
    void visit(IfExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->condition->accept(this);
        node->thenBranch->accept(this);
        if (node->elseBranch) node->elseBranch->accept(this);
    }
    
    void visit(MatchExpr* node) override {
        visit(static_cast<Expression*>(node));
        node->subject->accept(this);
        for (auto* arm : node->arms) {
            arm->accept(this);
        }
    }
    
    void visit(MatchArm* node) override {
        visit(static_cast<Node*>(node));
        node->pattern->accept(this);
        node->result->accept(this);
    }
    
    void visit(ExpressionStmt* node) override {
        visit(static_cast<Statement*>(node));
        node->expression->accept(this);
    }
    
    void visit(ReturnStmt* node) override {
        visit(static_cast<Statement*>(node));
        if (node->value) node->value->accept(this);
    }
    
    void visit(BreakStmt* node) override { 
        visit(static_cast<Statement*>(node));
    }
    
    void visit(ContinueStmt* node) override { 
        visit(static_cast<Statement*>(node));
    }
    
    void visit(WhileStmt* node) override {
        visit(static_cast<Statement*>(node));
        node->condition->accept(this);
        node->body->accept(this);
    }
    
    void visit(ForStmt* node) override {
        visit(static_cast<Statement*>(node));
        if (node->initializer) node->initializer->accept(this);
        if (node->condition) node->condition->accept(this);
        for (auto* update : node->updates) {
            update->accept(this);
        }
        node->body->accept(this);
    }
    
    void visit(ForInStmt* node) override {
        visit(static_cast<Statement*>(node));
        node->iterator->accept(this);
        node->iterable->accept(this);
        if (node->indexVar) node->indexVar->accept(this);
        node->body->accept(this);
    }
    
    void visit(UsingDirective* node) override {
        visit(static_cast<Statement*>(node));
        for (auto* part : node->path) {
            part->accept(this);
        }
        if (node->alias) node->alias->accept(this);
        if (node->aliasedType) node->aliasedType->accept(this);
    }
    
    void visit(VariableDecl* node) override {
        visit(static_cast<Declaration*>(node));
        node->variable->accept(this);
        if (node->initializer) node->initializer->accept(this);
    }
    
    void visit(MemberVariableDecl* node) override {
        visit(static_cast<Declaration*>(node));
        node->name->accept(this);
        if (node->type) node->type->accept(this);
        if (node->initializer) node->initializer->accept(this);
        if (node->getter) node->getter->accept(this);
        if (node->setter) node->setter->accept(this);
    }
    
    void visit(ParameterDecl* node) override {
        visit(static_cast<Declaration*>(node));
        node->param->accept(this);
        if (node->defaultValue) node->defaultValue->accept(this);
    }
    
    void visit(GenericParamDecl* node) override {
        visit(static_cast<Declaration*>(node));
        node->name->accept(this);
        for (auto* constraint : node->constraints) {
            constraint->accept(this);
        }
    }
    
    void visit(FunctionDecl* node) override {
        visit(static_cast<Declaration*>(node));
        node->name->accept(this);
        for (auto* param : node->genericParams) {
            param->accept(this);
        }
        for (auto* param : node->parameters) {
            param->accept(this);
        }
        if (node->returnType) node->returnType->accept(this);
        if (node->body) node->body->accept(this);
    }
    
    void visit(ConstructorDecl* node) override {
        visit(static_cast<Declaration*>(node));
        for (auto* param : node->parameters) {
            param->accept(this);
        }
        node->body->accept(this);
    }
    
    void visit(PropertyAccessor* node) override {
        visit(static_cast<Node*>(node));
        if (auto* expr = std::get_if<Expression*>(&node->body)) {
            (*expr)->accept(this);
        } else if (auto* block = std::get_if<Block*>(&node->body)) {
            (*block)->accept(this);
        }
    }
    
    void visit(InheritFunctionDecl* node) override {
        visit(static_cast<Declaration*>(node));
        node->functionName->accept(this);
        for (auto* type : node->parameterTypes) {
            type->accept(this);
        }
    }
    
    void visit(EnumCaseDecl* node) override {
        visit(static_cast<Declaration*>(node));
        node->name->accept(this);
        for (auto* data : node->associatedData) {
            data->accept(this);
        }
    }
    
    void visit(TypeDecl* node) override {
        visit(static_cast<Declaration*>(node));
        node->name->accept(this);
        for (auto* param : node->genericParams) {
            param->accept(this);
        }
        for (auto* base : node->baseTypes) {
            base->accept(this);
        }
        for (auto* member : node->members) {
            member->accept(this);
        }
    }
    
    void visit(NamespaceDecl* node) override {
        visit(static_cast<Declaration*>(node));
        for (auto* part : node->path) {
            part->accept(this);
        }
        if (node->body) {
            for (auto* stmt : *node->body) {
                stmt->accept(this);
            }
        }
    }
    
    void visit(NamedTypeRef* node) override {
        visit(static_cast<TypeRef*>(node));
        for (auto* part : node->path) {
            part->accept(this);
        }
        for (auto* arg : node->genericArgs) {
            arg->accept(this);
        }
    }
    
    void visit(ArrayTypeRef* node) override {
        visit(static_cast<TypeRef*>(node));
        node->elementType->accept(this);
    }
    
    void visit(FunctionTypeRef* node) override {
        visit(static_cast<TypeRef*>(node));
        for (auto* param : node->parameterTypes) {
            param->accept(this);
        }
        if (node->returnType) node->returnType->accept(this);
    }
    
    void visit(NullableTypeRef* node) override {
        visit(static_cast<TypeRef*>(node));
        node->innerType->accept(this);
    }
    
    void visit(RefTypeRef* node) override {
        visit(static_cast<TypeRef*>(node));
        node->innerType->accept(this);
    }
    
    void visit(BaseTypeConstraint* node) override {
        visit(static_cast<TypeConstraint*>(node));
        node->baseType->accept(this);
    }
    
    void visit(ConstructorConstraint* node) override {
        visit(static_cast<TypeConstraint*>(node));
        for (auto* type : node->parameterTypes) {
            type->accept(this);
        }
    }

    void visit(TypeKindConstraint* node) override {
        visit(static_cast<TypeConstraint*>(node));
    }

    void visit(LiteralPattern* node) override {
        visit(static_cast<Pattern*>(node));
        node->literal->accept(this);
    }
    
    void visit(BindingPattern* node) override {
        visit(static_cast<Pattern*>(node));
        if (node->name) node->name->accept(this);
    }
    
    void visit(EnumPattern* node) override {
        visit(static_cast<Pattern*>(node));
        for (auto* part : node->path) {
            part->accept(this);
        }
        for (auto* pattern : node->argumentPatterns) {
            pattern->accept(this);
        }
    }
    
    void visit(RangePattern* node) override {
        visit(static_cast<Pattern*>(node));
        if (node->start) node->start->accept(this);
        if (node->end) node->end->accept(this);
    }
    
    void visit(InPattern* node) override {
        visit(static_cast<Pattern*>(node));
        node->innerPattern->accept(this);
    }
    
    void visit(ComparisonPattern* node) override {
        visit(static_cast<Pattern*>(node));
        node->value->accept(this);
    }
    
    void visit(CompilationUnit* node) override {
        visit(static_cast<Node*>(node));
        for (auto* stmt : node->topLevelStatements) {
            stmt->accept(this);
        }
    }
};

// Implementation of Node::accept (needed for base case)
inline void Node::accept(Visitor* visitor) {
    visitor->visit(this);
}

// Default implementations for base type visits
inline void Visitor::visit(Node* node) {
    // Default: do nothing - override in derived visitors for uniform handling
}

inline void Visitor::visit(Expression* node) {
    visit(static_cast<Node*>(node));
}

inline void Visitor::visit(Statement* node) {
    visit(static_cast<Node*>(node));
}

inline void Visitor::visit(Declaration* node) {
    visit(static_cast<Statement*>(node));
}

inline void Visitor::visit(Pattern* node) {
    visit(static_cast<Node*>(node));
}

inline void Visitor::visit(TypeRef* node) {
    visit(static_cast<Node*>(node));
}

inline void Visitor::visit(TypeConstraint* node) {
    visit(static_cast<Node*>(node));
}

} // namespace Myre