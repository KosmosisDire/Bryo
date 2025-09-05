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

namespace Bryo
{

    // ============================================================================
    // --- Forward Declarations ---
    // ============================================================================
    struct Token;

    struct BaseSyntax;
    struct BaseExprSyntax;
    struct BaseStmtSyntax;
    struct BaseDeclSyntax;
    class Visitor;
    
    // Add all concrete node forward declarations
    struct NameSyntax;
    struct IdentifierNameSyntax;
    struct QualifiedNameSyntax;
    struct GenericNameSyntax;
    struct PredefinedTypeSyntax;
    
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
    struct IndexerExpr;
    struct CastExpr;
    struct NewExpr;
    struct ThisExpr;
    struct LambdaExpr;
    struct ConditionalExpr;
    struct TypeOfExpr;
    struct SizeOfExpr;
    struct Block;
    struct IfStmt;
    
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
    struct PropertyAccessor;
    struct FunctionDecl;
    struct ParameterDecl;
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
        virtual void visit(BaseSyntax *node);
        virtual void visit(BaseExprSyntax *node);
        virtual void visit(BaseStmtSyntax *node);
        virtual void visit(BaseDeclSyntax *node);

        // All concrete node types
        virtual void visit(IdentifierNameSyntax *node) = 0;
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
        virtual void visit(QualifiedNameSyntax *node) = 0;
        virtual void visit(IndexerExpr *node) = 0;
        virtual void visit(CastExpr *node) = 0;
        virtual void visit(NewExpr *node) = 0;
        virtual void visit(ThisExpr *node) = 0;
        virtual void visit(LambdaExpr *node) = 0;
        virtual void visit(ConditionalExpr *node) = 0;
        virtual void visit(TypeOfExpr *node) = 0;
        virtual void visit(SizeOfExpr *node) = 0;
        virtual void visit(Block *node) = 0;
        virtual void visit(IfStmt *node) = 0;
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

    struct BaseSyntax
    {
        SourceRange location;
        SymbolHandle containingScope = {0};

        virtual ~BaseSyntax() = default;
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

    struct BaseExprSyntax : BaseSyntax
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

    struct BaseStmtSyntax : BaseSyntax
    {
        ACCEPT_VISITOR
    };

    // Base for declarations - no name field!
    struct BaseDeclSyntax : BaseStmtSyntax
    {
        ModifierKindFlags modifiers;
        ACCEPT_VISITOR
    };

    struct Pattern : BaseSyntax
    {
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Basic Building Blocks ---
    // ============================================================================

    struct IdentifierNameSyntax : BaseSyntax
    {
        std::string text;
        ACCEPT_VISITOR
    };

    // Reusable component for name + optional type
    struct TypedIdentifier : BaseSyntax
    {
        IdentifierNameSyntax *name;
        BaseExprSyntax *type; // null = inferred (var)
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Error Nodes (for robust error recovery) ---
    // ============================================================================

    struct ErrorExpression : BaseExprSyntax
    {
        std::string message;
        List<BaseSyntax *> partialNodes;
        ACCEPT_VISITOR
    };

    struct ErrorStatement : BaseStmtSyntax
    {
        std::string message;
        List<BaseSyntax *> partialNodes;
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Expressions ---
    // ============================================================================

    struct LiteralExpr : BaseExprSyntax
    {
        LiteralKind kind;
        std::string value; // Raw text from source
        ACCEPT_VISITOR
    };

    struct ArrayLiteralExpr : BaseExprSyntax
    {
        List<BaseExprSyntax *> elements;
        ACCEPT_VISITOR
    };

    struct NameExpr : BaseExprSyntax
    {
        SymbolHandle resolvedSymbol = {0};
        IdentifierNameSyntax *name; // Single identifier only - no multi-part names
        ACCEPT_VISITOR

        std::string get_name() const
        {
            return name ? std::string(name->text) : "";
        }
    };

    struct UnaryExpr : BaseExprSyntax
    {
        UnaryOperatorKind op;
        BaseExprSyntax *operand; // Never null (ErrorExpression if parse fails)
        bool isPostfix;
        ACCEPT_VISITOR
    };

    struct BinaryExpr : BaseExprSyntax
    {
        BaseExprSyntax *left; // Never null
        BinaryOperatorKind op;
        BaseExprSyntax *right; // Never null
        ACCEPT_VISITOR
    };

    struct AssignmentExpr : BaseExprSyntax
    {
        BaseExprSyntax *target; // Never null, must be lvalue
        AssignmentOperatorKind op;
        BaseExprSyntax *value; // Never null
        ACCEPT_VISITOR
    };

    struct CallExpr : BaseExprSyntax
    {
        SymbolHandle resolvedCallee = {0};
        BaseExprSyntax *callee; // Never null
        List<BaseExprSyntax *> arguments;
        ACCEPT_VISITOR
    };

    struct QualifiedNameSyntax : BaseExprSyntax
    {
        SymbolHandle resolvedMember = {0};
        BaseExprSyntax *object; // Never null
        IdentifierNameSyntax *member; // Never null
        ACCEPT_VISITOR
    };

    struct IndexerExpr : BaseExprSyntax
    {
        BaseExprSyntax *object; // Never null
        BaseExprSyntax *index;  // Never null
        ACCEPT_VISITOR
    };

    struct CastExpr : BaseExprSyntax
    {
        BaseExprSyntax *targetType; // Never null
        BaseExprSyntax *expression; // Never null
        ACCEPT_VISITOR
    };

    struct NewExpr : BaseExprSyntax
    {
        BaseExprSyntax *type; // Never null
        List<BaseExprSyntax *> arguments;
        ACCEPT_VISITOR
    };

    struct ThisExpr : BaseExprSyntax
    {
        ACCEPT_VISITOR
    };

    struct LambdaExpr : BaseExprSyntax
    {
        List<ParameterDecl *> parameters;
        BaseStmtSyntax *body; // Block or ExpressionStmt
        ACCEPT_VISITOR
    };

    struct ConditionalExpr : BaseExprSyntax
    {
        BaseExprSyntax *condition; // Never null
        BaseExprSyntax *thenExpr;  // Never null
        BaseExprSyntax *elseExpr;  // Never null
        ACCEPT_VISITOR
    };

    struct TypeOfExpr : BaseExprSyntax
    {
        BaseExprSyntax *type; // Never null - now Expression instead of TypeRef
        ACCEPT_VISITOR
    };

    struct SizeOfExpr : BaseExprSyntax
    {
        BaseExprSyntax *type; // Never null - now Expression instead of TypeRef
        ACCEPT_VISITOR
    };

    // Type expressions (expressions that represent types in type contexts)
    struct ArrayTypeExpr : BaseExprSyntax
    {
        BaseExprSyntax *baseType; // Never null - the element type expression
        LiteralExpr *size;       // Can be null (size not type-checked), must be an integer literal if specified in the type
        ACCEPT_VISITOR
    };

    struct FunctionTypeExpr : BaseExprSyntax
    {
        List<BaseExprSyntax *> parameterTypes; // Parameter type expressions
        BaseExprSyntax *returnType;            // null = void - return type expression
        ACCEPT_VISITOR
    };

    struct GenericTypeExpr : BaseExprSyntax
    {
        BaseExprSyntax *baseType;               // The generic type (e.g., Array, Optional)
        List<BaseExprSyntax *> typeArguments;  // The type arguments (e.g., i32, String)
        ACCEPT_VISITOR
    };

    struct PointerTypeExpr : BaseExprSyntax
    {
        BaseExprSyntax *baseType; // Never null
        ACCEPT_VISITOR
    };

    // Single block type for both statements and expressions
    struct Block : BaseStmtSyntax
    {
        List<BaseStmtSyntax *> statements;
        ACCEPT_VISITOR
    };

    struct IfStmt : BaseStmtSyntax
    {
        BaseExprSyntax *condition; // Never null
        BaseStmtSyntax *thenBranch; // Never null (usually Block)
        BaseStmtSyntax *elseBranch; // Can be null
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Statements ---
    // ============================================================================

    struct ExpressionStmt : BaseStmtSyntax
    {
        BaseExprSyntax *expression; // Never null
        ACCEPT_VISITOR
    };

    struct ReturnStmt : BaseStmtSyntax
    {
        BaseExprSyntax *value; // Can be null (void return)
        ACCEPT_VISITOR
    };

    struct BreakStmt : BaseStmtSyntax
    {
        ACCEPT_VISITOR
    };

    struct ContinueStmt : BaseStmtSyntax
    {
        ACCEPT_VISITOR
    };

    struct WhileStmt : BaseStmtSyntax
    {
        BaseExprSyntax *condition; // Never null
        BaseStmtSyntax *body;       // Never null (usually Block)
        ACCEPT_VISITOR
    };

    struct ForStmt : BaseStmtSyntax
    {
        BaseStmtSyntax *initializer; // Can be null
        BaseExprSyntax *condition;  // Can be null (infinite loop)
        List<BaseExprSyntax *> updates;
        BaseStmtSyntax *body; // Never null
        ACCEPT_VISITOR
    };

    struct UsingDirective : BaseStmtSyntax
    {
        enum class Kind
        {
            Namespace, // using System.Collections;
            Alias      // using Dict = Dictionary<string, int>;
        };

        Kind kind;
        BaseExprSyntax *target; // The imported name (was path) - now uniform Expression

        // For alias only
        IdentifierNameSyntax *alias = nullptr;
        BaseExprSyntax *aliasedType = nullptr; // Now Expression instead of TypeRef
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Declarations ---
    // ============================================================================

    // Regular local variable: var x = 5;
    struct VariableDecl : BaseDeclSyntax
    {
        TypedIdentifier *variable; // Never null
        BaseExprSyntax *initializer;   // Can be null
        ACCEPT_VISITOR
    };

    // Property declaration: combines a variable with accessors
    struct PropertyDecl : BaseDeclSyntax
    {
        VariableDecl *variable;        // Never null - the underlying variable
        PropertyAccessor *getter;      // null = auto-generated get accessor
        PropertyAccessor *setter;      // null = no setter (read-only)
        ACCEPT_VISITOR
    };

    struct ParameterDecl : BaseDeclSyntax
    {
        TypedIdentifier *param;   // Never null
        BaseExprSyntax *defaultValue; // Can be null
        ACCEPT_VISITOR
    };

    struct FunctionDecl : BaseDeclSyntax
    {
        IdentifierNameSyntax *name; // Never null
        List<TypeParameterDecl *> typeParameters;
        List<ParameterDecl *> parameters;
        BaseExprSyntax *returnType;      // null = void - now Expression instead of TypeRef
        Block *body;                 // Can be null (abstract)
        SymbolHandle functionSymbol; // Unique ID for this function scope
        ACCEPT_VISITOR
    };

    struct ConstructorDecl : BaseDeclSyntax
    {
        // No name field - constructors are always "new"
        List<ParameterDecl *> parameters;
        Block *body; // Never null
        ACCEPT_VISITOR
    };

    struct PropertyAccessor : BaseSyntax
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
            BaseExprSyntax *,   // Expression-bodied: => expr
            Block *         // Block-bodied: { ... }
            >
            body;
        ACCEPT_VISITOR
    };

    struct EnumCaseDecl : BaseDeclSyntax
    {
        IdentifierNameSyntax *name;                     // Never null
        List<ParameterDecl *> associatedData; // Can be empty
        ACCEPT_VISITOR
    };

    struct TypeDecl : BaseDeclSyntax
    {
        enum class Kind
        {
            Type,       // type
            RefType,    // ref type
            StaticType, // static type (all members implicitly static)
            Enum        // enum
        };

        IdentifierNameSyntax *name; // Never null
        Kind kind;
        List<TypeParameterDecl *> typeParameters;
        List<BaseExprSyntax *> baseTypes; // Now Expression instead of TypeRef
        List<BaseDeclSyntax *> members;
        ACCEPT_VISITOR
    };

    struct TypeParameterDecl : BaseDeclSyntax
    {
        IdentifierNameSyntax *name; // Never null - the type parameter name (T, U, etc.)
        // Future: constraints can be added here
        ACCEPT_VISITOR
    };

    struct NamespaceDecl : BaseDeclSyntax
    {
        BaseExprSyntax *name; // Never null - now uniform Expression instead of path
        bool isFileScoped;
        std::optional<List<BaseStmtSyntax *>> body; // nullopt for file-scoped
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Type System (Removed - types are now expressions) ---
    // ============================================================================

    // ============================================================================
    // --- Root Node ---
    // ============================================================================

    struct CompilationUnit : BaseSyntax
    {
        List<BaseStmtSyntax *> topLevelStatements;
        ACCEPT_VISITOR
    };

    // ============================================================================
    // --- Default Visitor (with automatic traversal) ---
    // ============================================================================

    class DefaultVisitor : public Visitor
    {
    public:
        // Base type visit implementations - override these for uniform handling
        void visit(BaseSyntax *node) override { /* default: do nothing */ }
        void visit(BaseExprSyntax *node) override { visit(static_cast<BaseSyntax *>(node)); }
        void visit(BaseStmtSyntax *node) override { visit(static_cast<BaseSyntax *>(node)); }
        void visit(BaseDeclSyntax *node) override { visit(static_cast<BaseStmtSyntax *>(node)); }

        // Default implementations that traverse children
        void visit(IdentifierNameSyntax *node) override { visit(static_cast<BaseSyntax *>(node)); }

        void visit(TypedIdentifier *node) override
        {
            visit(static_cast<BaseSyntax *>(node));
            if (node->name)
                node->name->accept(this);
            if (node->type)
                node->type->accept(this);
        }

        void visit(ErrorExpression *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            for (auto partial : node->partialNodes)
            {
                if (partial)
                    partial->accept(this);
            }
        }

        void visit(ErrorStatement *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
            for (auto partial : node->partialNodes)
            {
                if (partial)
                    partial->accept(this);
            }
        }

        void visit(LiteralExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
        }

        void visit(ArrayLiteralExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            for (auto elem : node->elements)
            {
                elem->accept(this);
            }
        }

        void visit(NameExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            if (node->name)
                node->name->accept(this);
        }

        void visit(UnaryExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->operand->accept(this);
        }

        void visit(BinaryExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->left->accept(this);
            node->right->accept(this);
        }

        void visit(AssignmentExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->target->accept(this);
            node->value->accept(this);
        }

        void visit(CallExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->callee->accept(this);
            for (auto arg : node->arguments)
            {
                arg->accept(this);
            }
        }

        void visit(QualifiedNameSyntax *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->object->accept(this);
            node->member->accept(this);
        }

        void visit(IndexerExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->object->accept(this);
            node->index->accept(this);
        }

        void visit(CastExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->targetType->accept(this);
            node->expression->accept(this);
        }

        void visit(NewExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->type->accept(this);
            for (auto arg : node->arguments)
            {
                arg->accept(this);
            }
        }

        void visit(ThisExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
        }

        void visit(LambdaExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            for (auto param : node->parameters)
            {
                param->accept(this);
            }
            if (node->body)
                node->body->accept(this);
        }

        void visit(ConditionalExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->condition->accept(this);
            node->thenExpr->accept(this);
            node->elseExpr->accept(this);
        }

        void visit(TypeOfExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->type->accept(this);
        }

        void visit(SizeOfExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            node->type->accept(this);
        }

        void visit(Block *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
            for (auto stmt : node->statements)
            {
                stmt->accept(this);
            }
        }

        void visit(IfStmt *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
            node->condition->accept(this);
            node->thenBranch->accept(this);
            if (node->elseBranch)
                node->elseBranch->accept(this);
        }

        void visit(ArrayTypeExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
            if (node->baseType)
                node->baseType->accept(this);
            if (node->size)
                node->size->accept(this);
        }

        void visit(FunctionTypeExpr *node) override
        {
            visit(static_cast<BaseExprSyntax *>(node));
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
            visit(static_cast<BaseExprSyntax *>(node));
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
            visit(static_cast<BaseExprSyntax *>(node));
            node->baseType->accept(this);
        }

        void visit(ExpressionStmt *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
            node->expression->accept(this);
        }

        void visit(ReturnStmt *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
            if (node->value)
                node->value->accept(this);
        }

        void visit(BreakStmt *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
        }

        void visit(ContinueStmt *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
        }

        void visit(WhileStmt *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
            node->condition->accept(this);
            node->body->accept(this);
        }

        void visit(ForStmt *node) override
        {
            visit(static_cast<BaseStmtSyntax *>(node));
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
            visit(static_cast<BaseStmtSyntax *>(node));
            if (node->target)
                node->target->accept(this);
            if (node->alias)
                node->alias->accept(this);
            if (node->aliasedType)
                node->aliasedType->accept(this);
        }

        void visit(VariableDecl *node) override
        {
            visit(static_cast<BaseDeclSyntax *>(node));
            node->variable->accept(this);
            if (node->initializer)
                node->initializer->accept(this);
        }

        void visit(PropertyDecl *node) override
        {
            visit(static_cast<BaseDeclSyntax *>(node));
            node->variable->accept(this);
            if (node->getter)
                node->getter->accept(this);
            if (node->setter)
                node->setter->accept(this);
        }

        void visit(ParameterDecl *node) override
        {
            visit(static_cast<BaseDeclSyntax *>(node));
            node->param->accept(this);
            if (node->defaultValue)
                node->defaultValue->accept(this);
        }

        void visit(FunctionDecl *node) override
        {
            visit(static_cast<BaseDeclSyntax *>(node));
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
            visit(static_cast<BaseDeclSyntax *>(node));
            for (auto param : node->parameters)
            {
                param->accept(this);
            }
            node->body->accept(this);
        }

        void visit(PropertyAccessor *node) override
        {
            visit(static_cast<BaseSyntax *>(node));
            if (auto expr = std::get_if<BaseExprSyntax *>(&node->body))
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
            visit(static_cast<BaseDeclSyntax *>(node));
            node->name->accept(this);
            for (auto data : node->associatedData)
            {
                data->accept(this);
            }
        }

        void visit(TypeDecl *node) override
        {
            visit(static_cast<BaseDeclSyntax *>(node));
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
            visit(static_cast<BaseDeclSyntax *>(node));
            if (node->name)
                node->name->accept(this);
        }

        void visit(NamespaceDecl *node) override
        {
            visit(static_cast<BaseDeclSyntax *>(node));
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
            visit(static_cast<BaseSyntax *>(node));
            for (auto stmt : node->topLevelStatements)
            {
                stmt->accept(this);
            }
        }
    };

    // Implementation of Node::accept (needed for base case)
    inline void BaseSyntax::accept(Visitor *visitor)
    {
        visitor->visit(this);
    }

    // Default implementations for base type visits
    inline void Visitor::visit(BaseSyntax *node)
    {
        // Default: do nothing - override in derived visitors for uniform handling
    }

    inline void Visitor::visit(BaseExprSyntax *node)
    {
        visit(static_cast<BaseSyntax *>(node));
    }

    inline void Visitor::visit(BaseStmtSyntax *node)
    {
        visit(static_cast<BaseSyntax *>(node));
    }

    inline void Visitor::visit(BaseDeclSyntax *node)
    {
        visit(static_cast<BaseStmtSyntax *>(node));
    }

} // namespace Bryo