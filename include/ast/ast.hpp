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
#include "semantic/type.hpp"

namespace Myre
{

    // ============================================================================
    // --- Forward Declarations ---
    // ============================================================================
    struct Token;

    struct Node;
    struct Expression;
    struct Statement;
    struct Declaration;
    struct PropertyAccessor;
    struct ParameterDecl;
    class Visitor;

    // Add all concrete node forward declarations
    struct Identifier;
    struct TypedIdentifier;
    struct ErrorExpression;
    struct ErrorStatement;
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
    struct ConditionalExpr;
    struct TypeOfExpr;
    struct SizeOfExpr;
    struct Block;
    struct IfExpr;

    // Type expressions (expressions that represent types)
    struct ArrayTypeExpr;
    struct FunctionTypeExpr;
    struct GenericTypeExpr;
    struct PointerTypeExpr;

    // Statements
    struct ExpressionStmt;
    struct ReturnStmt;
    struct BreakStmt;
    struct ContinueStmt;
    struct WhileStmt;
    struct ForStmt;
    struct UsingDirective;

    // Declarations
    struct VariableDecl;
    struct PropertyDecl;
    struct FunctionDecl;
    struct ConstructorDecl;
    struct EnumCaseDecl;
    struct TypeDecl;
    struct TypeParameterDecl;
    struct NamespaceDecl;

    // Root
    struct CompilationUnit;

    // Non-owning collection type (arena allocated)
    template <typename T>
    using List = std::span<T>;

// Macro to create an accept function implementation
#define ACCEPT_VISITOR \
    void accept(Visitor *visitor) override { visitor->visit(this); }

    // ============================================================================
    // --- Visitor Pattern ---
    // ============================================================================

    class Visitor
    {
    public:
        virtual ~Visitor() = default;

        // Base type visits - can be overridden for uniform handling
        virtual void visit(Node *node);
        virtual void visit(Expression *node);
        virtual void visit(Statement *node);
        virtual void visit(Declaration *node);

        // All concrete node types
        virtual void visit(Identifier *node) = 0;
        virtual void visit(TypedIdentifier *node) = 0;
        virtual void visit(ErrorExpression *node) = 0;
        virtual void visit(ErrorStatement *node) = 0;

        // Expressions
        virtual void visit(LiteralExpr *node) = 0;
        virtual void visit(ArrayLiteralExpr *node) = 0;
        virtual void visit(NameExpr *node) = 0;
        virtual void visit(UnaryExpr *node) = 0;
        virtual void visit(BinaryExpr *node) = 0;
        virtual void visit(AssignmentExpr *node) = 0;
        virtual void visit(CallExpr *node) = 0;
        virtual void visit(MemberAccessExpr *node) = 0;
        virtual void visit(IndexerExpr *node) = 0;
        virtual void visit(CastExpr *node) = 0;
        virtual void visit(NewExpr *node) = 0;
        virtual void visit(ThisExpr *node) = 0;
        virtual void visit(LambdaExpr *node) = 0;
        virtual void visit(ConditionalExpr *node) = 0;
        virtual void visit(TypeOfExpr *node) = 0;
        virtual void visit(SizeOfExpr *node) = 0;
        virtual void visit(Block *node) = 0;
        virtual void visit(IfExpr *node) = 0;
        virtual void visit(ArrayTypeExpr *node) = 0;
        virtual void visit(FunctionTypeExpr *node) = 0;
        virtual void visit(GenericTypeExpr *node) = 0;
        virtual void visit(PointerTypeExpr *node) = 0;

        // Statements
        virtual void visit(ExpressionStmt *node) = 0;
        virtual void visit(ReturnStmt *node) = 0;
        virtual void visit(BreakStmt *node) = 0;
        virtual void visit(ContinueStmt *node) = 0;
        virtual void visit(WhileStmt *node) = 0;
        virtual void visit(ForStmt *node) = 0;
        virtual void visit(UsingDirective *node) = 0;

        // Declarations
        virtual void visit(VariableDecl *node) = 0;
        virtual void visit(PropertyDecl *node) = 0;
        virtual void visit(ParameterDecl *node) = 0;
        virtual void visit(FunctionDecl *node) = 0;
        virtual void visit(ConstructorDecl *node) = 0;
        virtual void visit(PropertyAccessor *node) = 0;
        virtual void visit(EnumCaseDecl *node) = 0;
        virtual void visit(TypeDecl *node) = 0;
        virtual void visit(TypeParameterDecl *node) = 0;
        virtual void visit(NamespaceDecl *node) = 0;

        // Root
        virtual void visit(CompilationUnit *node) = 0;
    };

    // ============================================================================
    // --- Core Node Hierarchy ---
    // ============================================================================

    struct Node
    {
        SourceRange location;
        SymbolHandle containingScope = {0};

        virtual ~Node() = default;
        virtual void accept(Visitor *visitor);

        // Clean API using dynamic_cast
        template <typename T>
        bool is() const
        {
            return dynamic_cast<const T *>(this) != nullptr;
        }

        template <typename T>
        T *as()
        {
            return dynamic_cast<T *>(this);
        }

        template <typename T>
        const T *as() const
        {
            return dynamic_cast<const T *>(this);
        }
    };

    struct Expression : Node
    {
        TypePtr resolvedType;
        bool isLValue = false;
        ACCEPT_VISITOR

        bool is_l_value() const
        {
            return isLValue;
        }

        bool is_r_value() const
        {
            return !isLValue;
        }
    };

    struct Statement : Node
    {
        ACCEPT_VISITOR
    };

    // Base for declarations - no name field!
    struct Declaration : Statement
    {
        ModifierKindFlags modifiers;
        ACCEPT_VISITOR
    };

    struct Pattern : Node
    {
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Basic Building Blocks ---
    // ============================================================================

    struct Identifier : Node
    {
        std::string text;
        ACCEPT_VISITOR
    };

    // Reusable component for name + optional type
    struct TypedIdentifier : Node
    {
        Identifier *name;
        Expression *type; // null = inferred (var)
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Error Nodes (for robust error recovery) ---
    // ============================================================================

    struct ErrorExpression : Expression
    {
        std::string message;
        List<Node *> partialNodes;
        ACCEPT_VISITOR
    };

    struct ErrorStatement : Statement
    {
        std::string message;
        List<Node *> partialNodes;
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Expressions ---
    // ============================================================================

    struct LiteralExpr : Expression
    {
        LiteralKind kind;
        std::string value; // Raw text from source
        ACCEPT_VISITOR
    };

    struct ArrayLiteralExpr : Expression
    {
        List<Expression *> elements;
        ACCEPT_VISITOR
    };

    struct NameExpr : Expression
    {
        SymbolHandle resolvedSymbol = {0};
        Identifier *name; // Single identifier only - no multi-part names
        ACCEPT_VISITOR

        std::string get_name() const
        {
            return name ? std::string(name->text) : "";
        }
    };

    struct UnaryExpr : Expression
    {
        UnaryOperatorKind op;
        Expression *operand; // Never null (ErrorExpression if parse fails)
        bool isPostfix;
        ACCEPT_VISITOR
    };

    struct BinaryExpr : Expression
    {
        Expression *left; // Never null
        BinaryOperatorKind op;
        Expression *right; // Never null
        ACCEPT_VISITOR
    };

    struct AssignmentExpr : Expression
    {
        Expression *target; // Never null, must be lvalue
        AssignmentOperatorKind op;
        Expression *value; // Never null
        ACCEPT_VISITOR
    };

    struct CallExpr : Expression
    {
        SymbolHandle resolvedCallee = {0};
        Expression *callee; // Never null
        List<Expression *> arguments;
        ACCEPT_VISITOR
    };

    struct MemberAccessExpr : Expression
    {
        SymbolHandle resolvedMember = {0};
        Expression *object; // Never null
        Identifier *member; // Never null
        ACCEPT_VISITOR
    };

    struct IndexerExpr : Expression
    {
        Expression *object; // Never null
        Expression *index;  // Never null
        ACCEPT_VISITOR
    };

    struct CastExpr : Expression
    {
        Expression *targetType; // Never null
        Expression *expression; // Never null
        ACCEPT_VISITOR
    };

    struct NewExpr : Expression
    {
        Expression *type; // Never null
        List<Expression *> arguments;
        ACCEPT_VISITOR
    };

    struct ThisExpr : Expression
    {
        ACCEPT_VISITOR
    };

    struct LambdaExpr : Expression
    {
        List<ParameterDecl *> parameters;
        Statement *body; // Block or ExpressionStmt
        ACCEPT_VISITOR
    };

    struct ConditionalExpr : Expression
    {
        Expression *condition; // Never null
        Expression *thenExpr;  // Never null
        Expression *elseExpr;  // Never null
        ACCEPT_VISITOR
    };

    struct TypeOfExpr : Expression
    {
        Expression *type; // Never null - now Expression instead of TypeRef
        ACCEPT_VISITOR
    };

    struct SizeOfExpr : Expression
    {
        Expression *type; // Never null - now Expression instead of TypeRef
        ACCEPT_VISITOR
    };

    // Type expressions (expressions that represent types in type contexts)
    struct ArrayTypeExpr : Expression
    {
        Expression *baseType; // Never null - the element type expression
        LiteralExpr *size;       // Can be null (size not type-checked), must be an integer literal if specified in the type
        ACCEPT_VISITOR
    };

    struct FunctionTypeExpr : Expression
    {
        List<Expression *> parameterTypes; // Parameter type expressions
        Expression *returnType;            // null = void - return type expression
        ACCEPT_VISITOR
    };

    struct GenericTypeExpr : Expression
    {
        Expression *baseType;               // The generic type (e.g., Array, Optional)
        List<Expression *> typeArguments;  // The type arguments (e.g., i32, String)
        ACCEPT_VISITOR
    };

    struct PointerTypeExpr : Expression
    {
        Expression *baseType; // Never null
        ACCEPT_VISITOR
    };

    // Single block type for both statements and expressions
    struct Block : Statement
    {
        List<Statement *> statements;
        ACCEPT_VISITOR
    };

    struct IfExpr : Statement
    {
        Expression *condition; // Never null
        Statement *thenBranch; // Never null (usually Block)
        Statement *elseBranch; // Can be null
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Statements ---
    // ============================================================================

    struct ExpressionStmt : Statement
    {
        Expression *expression; // Never null
        ACCEPT_VISITOR
    };

    struct ReturnStmt : Statement
    {
        Expression *value; // Can be null (void return)
        ACCEPT_VISITOR
    };

    struct BreakStmt : Statement
    {
        ACCEPT_VISITOR
    };

    struct ContinueStmt : Statement
    {
        ACCEPT_VISITOR
    };

    struct WhileStmt : Statement
    {
        Expression *condition; // Never null
        Statement *body;       // Never null (usually Block)
        ACCEPT_VISITOR
    };

    struct ForStmt : Statement
    {
        Statement *initializer; // Can be null
        Expression *condition;  // Can be null (infinite loop)
        List<Expression *> updates;
        Statement *body; // Never null
        ACCEPT_VISITOR
    };

    struct UsingDirective : Statement
    {
        enum class Kind
        {
            Namespace, // using System.Collections;
            Alias      // using Dict = Dictionary<string, int>;
        };

        Kind kind;
        Expression *target; // The imported name (was path) - now uniform Expression

        // For alias only
        Identifier *alias = nullptr;
        Expression *aliasedType = nullptr; // Now Expression instead of TypeRef
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Declarations ---
    // ============================================================================

    // Regular local variable: var x = 5;
    struct VariableDecl : Declaration
    {
        TypedIdentifier *variable; // Never null
        Expression *initializer;   // Can be null
        ACCEPT_VISITOR
    };

    // Property declaration: combines a variable with accessors
    struct PropertyDecl : Declaration
    {
        VariableDecl *variable;        // Never null - the underlying variable
        PropertyAccessor *getter;      // null = auto-generated get accessor
        PropertyAccessor *setter;      // null = no setter (read-only)
        ACCEPT_VISITOR
    };

    struct ParameterDecl : Declaration
    {
        TypedIdentifier *param;   // Never null
        Expression *defaultValue; // Can be null
        ACCEPT_VISITOR
    };

    struct FunctionDecl : Declaration
    {
        Identifier *name; // Never null
        List<TypeParameterDecl *> typeParameters;
        List<ParameterDecl *> parameters;
        Expression *returnType;      // null = void - now Expression instead of TypeRef
        Block *body;                 // Can be null (abstract)
        SymbolHandle functionSymbol; // Unique ID for this function scope
        ACCEPT_VISITOR
    };

    struct ConstructorDecl : Declaration
    {
        // No name field - constructors are always "new"
        List<ParameterDecl *> parameters;
        Block *body; // Never null
        ACCEPT_VISITOR
    };

    struct PropertyAccessor : Node
    {
        enum class Kind
        {
            Get,
            Set
        };
        Kind kind;
        ModifierKindFlags modifiers;

        // Body representation
        std::variant<
            std::monostate, // Default/auto-implemented
            Expression *,   // Expression-bodied: => expr
            Block *         // Block-bodied: { ... }
            >
            body;
        ACCEPT_VISITOR
    };

    struct EnumCaseDecl : Declaration
    {
        Identifier *name;                     // Never null
        List<ParameterDecl *> associatedData; // Can be empty
        ACCEPT_VISITOR
    };

    struct TypeDecl : Declaration
    {
        enum class Kind
        {
            Type,       // type
            ValueType,  // value type
            RefType,    // ref type
            StaticType, // static type (all members implicitly static)
            Enum        // enum
        };

        Identifier *name; // Never null
        Kind kind;
        List<TypeParameterDecl *> typeParameters;
        List<Expression *> baseTypes; // Now Expression instead of TypeRef
        List<Declaration *> members;
        ACCEPT_VISITOR
    };

    struct TypeParameterDecl : Declaration
    {
        Identifier *name; // Never null - the type parameter name (T, U, etc.)
        // Future: constraints can be added here
        ACCEPT_VISITOR
    };

    struct NamespaceDecl : Declaration
    {
        Expression *name; // Never null - now uniform Expression instead of path
        bool isFileScoped;
        std::optional<List<Statement *>> body; // nullopt for file-scoped
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Type System (Removed - types are now expressions) ---
    // ============================================================================

    // ============================================================================
    // --- Root Node ---
    // ============================================================================

    struct CompilationUnit : Node
    {
        List<Statement *> topLevelStatements;
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Default Visitor (with automatic traversal) ---
    // ============================================================================

    class DefaultVisitor : public Visitor
    {
    public:
        // Base type visit implementations - override these for uniform handling
        void visit(Node *node) override { /* default: do nothing */ }
        void visit(Expression *node) override { visit(static_cast<Node *>(node)); }
        void visit(Statement *node) override { visit(static_cast<Node *>(node)); }
        void visit(Declaration *node) override { visit(static_cast<Statement *>(node)); }

        // Default implementations that traverse children
        void visit(Identifier *node) override { visit(static_cast<Node *>(node)); }

        void visit(TypedIdentifier *node) override
        {
            visit(static_cast<Node *>(node));
            if (node->name)
                node->name->accept(this);
            if (node->type)
                node->type->accept(this);
        }

        void visit(ErrorExpression *node) override
        {
            visit(static_cast<Expression *>(node));
            for (auto partial : node->partialNodes)
            {
                if (partial)
                    partial->accept(this);
            }
        }

        void visit(ErrorStatement *node) override
        {
            visit(static_cast<Statement *>(node));
            for (auto partial : node->partialNodes)
            {
                if (partial)
                    partial->accept(this);
            }
        }

        void visit(LiteralExpr *node) override
        {
            visit(static_cast<Expression *>(node));
        }

        void visit(ArrayLiteralExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            for (auto elem : node->elements)
            {
                elem->accept(this);
            }
        }

        void visit(NameExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            if (node->name)
                node->name->accept(this);
        }

        void visit(UnaryExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->operand->accept(this);
        }

        void visit(BinaryExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->left->accept(this);
            node->right->accept(this);
        }

        void visit(AssignmentExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->target->accept(this);
            node->value->accept(this);
        }

        void visit(CallExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->callee->accept(this);
            for (auto arg : node->arguments)
            {
                arg->accept(this);
            }
        }

        void visit(MemberAccessExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->object->accept(this);
            node->member->accept(this);
        }

        void visit(IndexerExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->object->accept(this);
            node->index->accept(this);
        }

        void visit(CastExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->targetType->accept(this);
            node->expression->accept(this);
        }

        void visit(NewExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->type->accept(this);
            for (auto arg : node->arguments)
            {
                arg->accept(this);
            }
        }

        void visit(ThisExpr *node) override
        {
            visit(static_cast<Expression *>(node));
        }

        void visit(LambdaExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            for (auto param : node->parameters)
            {
                param->accept(this);
            }
            if (node->body)
                node->body->accept(this);
        }

        void visit(ConditionalExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->condition->accept(this);
            node->thenExpr->accept(this);
            node->elseExpr->accept(this);
        }

        void visit(TypeOfExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->type->accept(this);
        }

        void visit(SizeOfExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->type->accept(this);
        }

        void visit(Block *node) override
        {
            visit(static_cast<Statement *>(node));
            for (auto stmt : node->statements)
            {
                stmt->accept(this);
            }
        }

        void visit(IfExpr *node) override
        {
            visit(static_cast<Statement *>(node));
            node->condition->accept(this);
            node->thenBranch->accept(this);
            if (node->elseBranch)
                node->elseBranch->accept(this);
        }

        void visit(ArrayTypeExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            if (node->baseType)
                node->baseType->accept(this);
            if (node->size)
                node->size->accept(this);
        }

        void visit(FunctionTypeExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            for (auto paramType : node->parameterTypes)
            {
                if (paramType)
                    paramType->accept(this);
            }
            if (node->returnType)
                node->returnType->accept(this);
        }

        void visit(GenericTypeExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            if (node->baseType)
                node->baseType->accept(this);
            for (auto typeArg : node->typeArguments)
            {
                if (typeArg)
                    typeArg->accept(this);
            }
        }

        void visit(PointerTypeExpr *node) override
        {
            visit(static_cast<Expression *>(node));
            node->baseType->accept(this);
        }

        void visit(ExpressionStmt *node) override
        {
            visit(static_cast<Statement *>(node));
            node->expression->accept(this);
        }

        void visit(ReturnStmt *node) override
        {
            visit(static_cast<Statement *>(node));
            if (node->value)
                node->value->accept(this);
        }

        void visit(BreakStmt *node) override
        {
            visit(static_cast<Statement *>(node));
        }

        void visit(ContinueStmt *node) override
        {
            visit(static_cast<Statement *>(node));
        }

        void visit(WhileStmt *node) override
        {
            visit(static_cast<Statement *>(node));
            node->condition->accept(this);
            node->body->accept(this);
        }

        void visit(ForStmt *node) override
        {
            visit(static_cast<Statement *>(node));
            if (node->initializer)
                node->initializer->accept(this);
            if (node->condition)
                node->condition->accept(this);
            for (auto update : node->updates)
            {
                update->accept(this);
            }
            node->body->accept(this);
        }

        void visit(UsingDirective *node) override
        {
            visit(static_cast<Statement *>(node));
            if (node->target)
                node->target->accept(this);
            if (node->alias)
                node->alias->accept(this);
            if (node->aliasedType)
                node->aliasedType->accept(this);
        }

        void visit(VariableDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            node->variable->accept(this);
            if (node->initializer)
                node->initializer->accept(this);
        }

        void visit(PropertyDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            node->variable->accept(this);
            if (node->getter)
                node->getter->accept(this);
            if (node->setter)
                node->setter->accept(this);
        }

        void visit(ParameterDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            node->param->accept(this);
            if (node->defaultValue)
                node->defaultValue->accept(this);
        }

        void visit(FunctionDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            node->name->accept(this);
            for (auto typeParam : node->typeParameters)
            {
                if (typeParam)
                    typeParam->accept(this);
            }
            for (auto param : node->parameters)
            {
                param->accept(this);
            }
            if (node->returnType)
                node->returnType->accept(this);
            if (node->body)
                node->body->accept(this);
        }

        void visit(ConstructorDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            for (auto param : node->parameters)
            {
                param->accept(this);
            }
            node->body->accept(this);
        }

        void visit(PropertyAccessor *node) override
        {
            visit(static_cast<Node *>(node));
            if (auto expr = std::get_if<Expression *>(&node->body))
            {
                (*expr)->accept(this);
            }
            else if (auto block = std::get_if<Block *>(&node->body))
            {
                (*block)->accept(this);
            }
        }

        void visit(EnumCaseDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            node->name->accept(this);
            for (auto data : node->associatedData)
            {
                data->accept(this);
            }
        }

        void visit(TypeDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            node->name->accept(this);
            for (auto typeParam : node->typeParameters)
            {
                if (typeParam)
                    typeParam->accept(this);
            }
            for (auto base : node->baseTypes)
            {
                base->accept(this);
            }
            for (auto member : node->members)
            {
                member->accept(this);
            }
        }

        void visit(TypeParameterDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            if (node->name)
                node->name->accept(this);
        }

        void visit(NamespaceDecl *node) override
        {
            visit(static_cast<Declaration *>(node));
            if (node->name)
                node->name->accept(this);
            if (node->body)
            {
                for (auto stmt : *node->body)
                {
                    stmt->accept(this);
                }
            }
        }

        void visit(CompilationUnit *node) override
        {
            visit(static_cast<Node *>(node));
            for (auto stmt : node->topLevelStatements)
            {
                stmt->accept(this);
            }
        }
    };

    // Implementation of Node::accept (needed for base case)
    inline void Node::accept(Visitor *visitor)
    {
        visitor->visit(this);
    }

    // Default implementations for base type visits
    inline void Visitor::visit(Node *node)
    {
        // Default: do nothing - override in derived visitors for uniform handling
    }

    inline void Visitor::visit(Expression *node)
    {
        visit(static_cast<Node *>(node));
    }

    inline void Visitor::visit(Statement *node)
    {
        visit(static_cast<Node *>(node));
    }

    inline void Visitor::visit(Declaration *node)
    {
        visit(static_cast<Statement *>(node));
    }

} // namespace Myre