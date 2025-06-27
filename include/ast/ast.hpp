#pragma once

#include <cstdint>
#include <string>
#include <cstring> // for memcpy
#include "ast_rtti.hpp"
#include "ast_enums.hpp"
#include "ast_allocator.hpp"

// Define this to include a parent pointer in each AST node, which can be useful
// for analysis but adds memory overhead.
// #define AST_HAS_PARENT_POINTER

namespace Mycelium::Scripting::Lang
{
    // --- Forward Declarations ---
    // This comprehensive list allows any node to reference any other node.
    struct AstNode;
    struct TokenNode;
    struct IdentifierNode;
    struct ExpressionNode;
    struct StatementNode;
    struct DeclarationNode;

    // Expressions
    struct LiteralExpressionNode;
    struct IdentifierExpressionNode;
    struct ParenthesizedExpressionNode;
    struct UnaryExpressionNode;
    struct BinaryExpressionNode;
    struct AssignmentExpressionNode;
    struct CallExpressionNode;
    struct MemberAccessExpressionNode;
    struct NewExpressionNode;
    struct ThisExpressionNode;
    struct CastExpressionNode;
    struct IndexerExpressionNode;
    struct TypeOfExpressionNode;
    struct SizeOfExpressionNode;

    // Statements
    struct BlockStatementNode;
    struct ExpressionStatementNode;
    struct IfStatementNode;
    struct WhileStatementNode;
    struct ForStatementNode;
    struct ReturnStatementNode;
    struct BreakStatementNode;
    struct ContinueStatementNode;
    struct EmptyStatementNode;

    // Declarations
    struct NamespaceDeclarationNode;
    struct UsingDirectiveNode;
    struct TypeDeclarationNode;
    struct ClassDeclarationNode;
    struct StructDeclarationNode;
    struct InterfaceDeclarationNode;
    struct EnumDeclarationNode;
    struct MemberDeclarationNode;
    struct FieldDeclarationNode;
    struct FunctionDeclarationNode;
    struct ParameterNode;
    struct VariableDeclarationNode;
    struct LocalVariableDeclarationNode;
    struct CompilationUnitNode;

    // Types
    struct TypeNameNode;
    struct QualifiedTypeNameNode;
    struct PointerTypeNameNode;
    struct ArrayTypeNameNode;
    struct GenericTypeNameNode;
    struct GenericParameterNode;


    // --- SizedArray Utility ---
    // A simple, non-owning array view for collections of AST nodes.
    // Memory is managed by the AstAllocator.
    template <typename T>
    struct SizedArray
    {
        T* values;
        int size;

        SizedArray() : values(nullptr), size(0) {}

        T& operator[](int index) const {
            // Add assertion for bounds checking in debug builds
            return values[index];
        }

        T* begin() const { return values; }
        T* end() const { return values + size; }
        bool empty() const { return size == 0; }
        T back() const { return values[size - 1]; }
    };


    // --- Structural Visitor ---
    // The base class for all AST traversal and analysis passes.
    // It uses the RTTI system for dispatch.
    class StructuralVisitor
    {
    public:
        virtual ~StructuralVisitor() = default;

        // The default visit function, which all others will call.
        virtual void visit(AstNode* node);

        // One visit method for each concrete AND abstract node type.
        virtual void visit(TokenNode* node);
        virtual void visit(IdentifierNode* node);

        // Expressions
        virtual void visit(ExpressionNode* node);
        virtual void visit(LiteralExpressionNode* node);
        virtual void visit(IdentifierExpressionNode* node);
        virtual void visit(ParenthesizedExpressionNode* node);
        virtual void visit(UnaryExpressionNode* node);
        virtual void visit(BinaryExpressionNode* node);
        virtual void visit(AssignmentExpressionNode* node);
        virtual void visit(CallExpressionNode* node);
        virtual void visit(MemberAccessExpressionNode* node);
        virtual void visit(NewExpressionNode* node);
        virtual void visit(ThisExpressionNode* node);
        virtual void visit(CastExpressionNode* node);
        virtual void visit(IndexerExpressionNode* node);
        virtual void visit(TypeOfExpressionNode* node);
        virtual void visit(SizeOfExpressionNode* node);
        
        // Statements
        virtual void visit(StatementNode* node);
        virtual void visit(BlockStatementNode* node);
        virtual void visit(ExpressionStatementNode* node);
        virtual void visit(IfStatementNode* node);
        virtual void visit(WhileStatementNode* node);
        virtual void visit(ForStatementNode* node);
        virtual void visit(ReturnStatementNode* node);
        virtual void visit(BreakStatementNode* node);
        virtual void visit(ContinueStatementNode* node);
        virtual void visit(EmptyStatementNode* node);
        virtual void visit(LocalVariableDeclarationNode* node);

        // Declarations
        virtual void visit(DeclarationNode* node);
        virtual void visit(NamespaceDeclarationNode* node);
        virtual void visit(UsingDirectiveNode* node);
        virtual void visit(TypeDeclarationNode* node);
        virtual void visit(ClassDeclarationNode* node);
        virtual void visit(StructDeclarationNode* node);
        virtual void visit(InterfaceDeclarationNode* node);
        virtual void visit(EnumDeclarationNode* node);
        virtual void visit(MemberDeclarationNode* node);
        virtual void visit(FieldDeclarationNode* node);
        virtual void visit(FunctionDeclarationNode* node);
        virtual void visit(ParameterNode* node);
        virtual void visit(VariableDeclarationNode* node);
        virtual void visit(GenericParameterNode* node);
        
        // Types
        virtual void visit(TypeNameNode* node);
        virtual void visit(QualifiedTypeNameNode* node);
        virtual void visit(PointerTypeNameNode* node);
        virtual void visit(ArrayTypeNameNode* node);
        virtual void visit(GenericTypeNameNode* node);
        
        // Root
        virtual void visit(CompilationUnitNode* node);
    };


    // --- Base AST Node ---
    struct AstNode
    {
        AST_ROOT_TYPE(AstNode) // Root node has no base type

        #ifdef AST_HAS_PARENT_POINTER
        AstNode* parent;
        #endif

        uint8_t typeId;
        TokenKind tokenKind;
        int sourceStart;
        int sourceLength;
        int triviaStart;

        // Must be implemented in ast.cpp
        void init_with_type_id(uint8_t id);
        void accept(StructuralVisitor* visitor);
        std::string_view to_string_view() const;

        // RTTI functions defined at the end of this file
        template <typename T> bool is_a() { return node_is_a<T>(this); }
        template <typename T> T* as() { return node_dyn_cast<T>(this); }
    };


    // --- Node Hierarchy ---

    struct TokenNode : AstNode
    {
        AST_TYPE(TokenNode, AstNode)
        std::string_view text;
    };

    struct IdentifierNode : AstNode
    {
        AST_TYPE(IdentifierNode, AstNode)
        std::string_view name;
    };

    // --- Expressions ---
    struct ExpressionNode : AstNode { AST_TYPE(ExpressionNode, AstNode) };

    struct LiteralExpressionNode : ExpressionNode
    {
        AST_TYPE(LiteralExpressionNode, ExpressionNode)
        LiteralKind kind;
        TokenNode* token; // Contains the raw text
    };

    struct IdentifierExpressionNode : ExpressionNode
    {
        AST_TYPE(IdentifierExpressionNode, ExpressionNode)
        IdentifierNode* identifier;
    };

    struct ParenthesizedExpressionNode : ExpressionNode
    {
        AST_TYPE(ParenthesizedExpressionNode, ExpressionNode)
        TokenNode* openParen;
        ExpressionNode* expression;
        TokenNode* closeParen;
    };

    struct UnaryExpressionNode : ExpressionNode
    {
        AST_TYPE(UnaryExpressionNode, ExpressionNode)
        UnaryOperatorKind opKind;
        TokenNode* operatorToken;
        ExpressionNode* operand;
        bool isPostfix;
    };

    struct BinaryExpressionNode : ExpressionNode
    {
        AST_TYPE(BinaryExpressionNode, ExpressionNode)
        ExpressionNode* left;
        BinaryOperatorKind opKind;
        TokenNode* operatorToken;
        ExpressionNode* right;
    };

    struct AssignmentExpressionNode : ExpressionNode
    {
        AST_TYPE(AssignmentExpressionNode, ExpressionNode)
        ExpressionNode* target;
        AssignmentOperatorKind opKind;
        TokenNode* operatorToken;
        ExpressionNode* source;
    };

    struct CallExpressionNode : ExpressionNode
    {
        AST_TYPE(CallExpressionNode, ExpressionNode)
        ExpressionNode* target;
        TokenNode* openParen;
        SizedArray<ExpressionNode*> arguments;
        SizedArray<TokenNode*> commas;
        TokenNode* closeParen;
    };

    struct MemberAccessExpressionNode : ExpressionNode
    {
        AST_TYPE(MemberAccessExpressionNode, ExpressionNode)
        ExpressionNode* target;
        TokenNode* dotToken;
        IdentifierNode* member;
    };

    struct NewExpressionNode : ExpressionNode
    {
        AST_TYPE(NewExpressionNode, ExpressionNode)
        TokenNode* newKeyword;
        TypeNameNode* type;
        CallExpressionNode* constructorCall; // Optional, can be null
    };

    struct ThisExpressionNode : ExpressionNode
    {
        AST_TYPE(ThisExpressionNode, ExpressionNode)
        TokenNode* thisKeyword;
    };

    struct CastExpressionNode : ExpressionNode
    {
        AST_TYPE(CastExpressionNode, ExpressionNode)
        TokenNode* openParen;
        TypeNameNode* targetType;
        TokenNode* closeParen;
        ExpressionNode* expression;
    };

    struct IndexerExpressionNode : ExpressionNode
    {
        AST_TYPE(IndexerExpressionNode, ExpressionNode)
        ExpressionNode* target;
        TokenNode* openBracket;
        ExpressionNode* index;
        TokenNode* closeBracket;
    };

    struct TypeOfExpressionNode : ExpressionNode
    {
        AST_TYPE(TypeOfExpressionNode, ExpressionNode)
        TokenNode* typeOfKeyword;
        TokenNode* openParen;
        TypeNameNode* type;
        TokenNode* closeParen;
    };

    struct SizeOfExpressionNode : ExpressionNode
    {
        AST_TYPE(SizeOfExpressionNode, ExpressionNode)
        TokenNode* sizeOfKeyword;
        TokenNode* openParen;
        TypeNameNode* type;
        TokenNode* closeParen;
    };

    // --- Statements ---
    struct StatementNode : AstNode { AST_TYPE(StatementNode, AstNode) };

    struct EmptyStatementNode : StatementNode
    {
        AST_TYPE(EmptyStatementNode, StatementNode)
        TokenNode* semicolon;
    };

    struct BlockStatementNode : StatementNode
    {
        AST_TYPE(BlockStatementNode, StatementNode)
        TokenNode* openBrace;
        // A block can contain both statements and local declarations
        SizedArray<StatementNode*> statements;
        TokenNode* closeBrace;
    };

    struct ExpressionStatementNode : StatementNode
    {
        AST_TYPE(ExpressionStatementNode, StatementNode)
        ExpressionNode* expression;
        TokenNode* semicolon;
    };

    struct IfStatementNode : StatementNode
    {
        AST_TYPE(IfStatementNode, StatementNode)
        TokenNode* ifKeyword;
        TokenNode* openParen;
        ExpressionNode* condition;
        TokenNode* closeParen;
        StatementNode* thenStatement;
        TokenNode* elseKeyword; // Optional, can be null
        StatementNode* elseStatement; // Optional, can be null
    };

    struct WhileStatementNode : StatementNode
    {
        AST_TYPE(WhileStatementNode, StatementNode)
        TokenNode* whileKeyword;
        TokenNode* openParen;
        ExpressionNode* condition;
        TokenNode* closeParen;
        StatementNode* body;
    };

    struct ForStatementNode : StatementNode
    {
        AST_TYPE(ForStatementNode, StatementNode)
        TokenNode* forKeyword;
        TokenNode* openParen;
        StatementNode* initializer; // LocalVariableDeclarationNode or ExpressionStatementNode
        ExpressionNode* condition;
        TokenNode* firstSemicolon;
        SizedArray<ExpressionNode*> incrementors;
        TokenNode* secondSemicolon;
        TokenNode* closeParen;
        StatementNode* body;
    };

    struct ReturnStatementNode : StatementNode
    {
        AST_TYPE(ReturnStatementNode, StatementNode)
        TokenNode* returnKeyword;
        ExpressionNode* expression; // Optional, can be null
        TokenNode* semicolon;
    };

    struct BreakStatementNode : StatementNode
    {
        AST_TYPE(BreakStatementNode, StatementNode)
        TokenNode* breakKeyword;
        TokenNode* semicolon;
    };

    struct ContinueStatementNode : StatementNode
    {
        AST_TYPE(ContinueStatementNode, StatementNode)
        TokenNode* continueKeyword;
        TokenNode* semicolon;
    };

    // --- Type Names ---
    struct TypeNameNode : AstNode
    {
        AST_TYPE(TypeNameNode, AstNode)
        IdentifierNode* identifier;
    };

    struct QualifiedTypeNameNode : TypeNameNode
    {
        AST_TYPE(QualifiedTypeNameNode, TypeNameNode)
        TypeNameNode* left;
        TokenNode* colonColonToken;
        IdentifierNode* right;
    };

    struct PointerTypeNameNode : TypeNameNode
    {
        AST_TYPE(PointerTypeNameNode, TypeNameNode)
        TypeNameNode* elementType;
        TokenNode* asterisk;
    };

    struct ArrayTypeNameNode : TypeNameNode
    {
        AST_TYPE(ArrayTypeNameNode, TypeNameNode)
        TypeNameNode* elementType;
        TokenNode* openBracket;
        TokenNode* closeBracket;
    };

    struct GenericTypeNameNode : TypeNameNode
    {
        AST_TYPE(GenericTypeNameNode, TypeNameNode)
        TypeNameNode* baseType;
        TokenNode* openAngle;
        SizedArray<TypeNameNode*> arguments;
        SizedArray<TokenNode*> commas;
        TokenNode* closeAngle;
    };

    // --- Declarations ---
    struct DeclarationNode : StatementNode
    {
        AST_TYPE(DeclarationNode, StatementNode)
        SizedArray<ModifierKind> modifiers;
        IdentifierNode* name;
    };

    struct ParameterNode : DeclarationNode
    {
        AST_TYPE(ParameterNode, DeclarationNode)
        TypeNameNode* type;
        TokenNode* equalsToken; // Optional, can be null
        ExpressionNode* defaultValue; // Optional, can be null
    };

    // Note: This node is now only for non-members like parameters and locals.
    struct VariableDeclarationNode : DeclarationNode
    {
        AST_TYPE(VariableDeclarationNode, DeclarationNode)
        TypeNameNode* type;
        TokenNode* equalsToken; // Optional, can be null
        ExpressionNode* initializer; // Optional, can be null
    };

    struct LocalVariableDeclarationNode : StatementNode
    {
        AST_TYPE(LocalVariableDeclarationNode, StatementNode)
        // Wraps one or more variable declarations for a local scope
        SizedArray<VariableDeclarationNode*> declarators;
        TokenNode* semicolon;
    };

    struct MemberDeclarationNode : DeclarationNode
    {
        AST_TYPE(MemberDeclarationNode, DeclarationNode)
    };
    
    // FIX: Redesigned FieldDeclarationNode to be self-contained
    struct FieldDeclarationNode : MemberDeclarationNode
    {
        AST_TYPE(FieldDeclarationNode, MemberDeclarationNode)
        TypeNameNode* type;
        // The 'name' is inherited from DeclarationNode.
        TokenNode* equalsToken;
        ExpressionNode* initializer;
        TokenNode* semicolon;
    };

    struct GenericParameterNode : DeclarationNode
    {
        AST_TYPE(GenericParameterNode, DeclarationNode)
        // Name is the only required part
    };

    struct FunctionDeclarationNode : MemberDeclarationNode
    {
        AST_TYPE(FunctionDeclarationNode, MemberDeclarationNode)
        TypeNameNode* returnType; // Can be null for constructors
        TokenNode* openGeneric; // <
        SizedArray<GenericParameterNode*> genericParameters;
        TokenNode* closeGeneric; // >
        TokenNode* openParen;
        SizedArray<ParameterNode*> parameters;
        TokenNode* closeParen;
        BlockStatementNode* body; // Can be null for extern/interface methods
        TokenNode* semicolon; // For methods without a body
    };

    struct TypeDeclarationNode : DeclarationNode
    {
        AST_TYPE(TypeDeclarationNode, DeclarationNode)
        TokenNode* keyword; // class, struct, etc.
        TokenNode* openGeneric; // <
        SizedArray<GenericParameterNode*> genericParameters;
        TokenNode* closeGeneric; // >
        TokenNode* baseListColon;
        SizedArray<TypeNameNode*> baseTypes;
        TokenNode* openBrace;
        SizedArray<MemberDeclarationNode*> members;
        TokenNode* closeBrace;
    };

    struct ClassDeclarationNode : TypeDeclarationNode { AST_TYPE(ClassDeclarationNode, TypeDeclarationNode) };
    struct StructDeclarationNode : TypeDeclarationNode { AST_TYPE(StructDeclarationNode, TypeDeclarationNode) };
    struct InterfaceDeclarationNode : TypeDeclarationNode { AST_TYPE(InterfaceDeclarationNode, TypeDeclarationNode) };
    struct EnumDeclarationNode : TypeDeclarationNode { AST_TYPE(EnumDeclarationNode, TypeDeclarationNode) }; // Members will be FieldDeclarations with initializers

    struct UsingDirectiveNode : StatementNode
    {
        AST_TYPE(UsingDirectiveNode, StatementNode)
        TokenNode* usingKeyword;
        TypeNameNode* namespaceName;
        TokenNode* semicolon;
    };

    struct NamespaceDeclarationNode : DeclarationNode
    {
        AST_TYPE(NamespaceDeclarationNode, DeclarationNode)
        TokenNode* namespaceKeyword;
        BlockStatementNode* body; // File-scoped namespaces might not have braces
    };

    // The root of a parsed file
    struct CompilationUnitNode : AstNode
    {
        AST_TYPE(CompilationUnitNode, AstNode)
        SizedArray<StatementNode*> statements; // Can contain usings, namespace, function, and class decls
    };


    // --- Inline RTTI Helper Implementations ---
    // These need to be defined here after AstNode is fully defined.
    // They are templated, so they must be in the header.

    template <typename T>
    inline bool node_is_a(AstNode* node)
    {
        if (node == nullptr)
            return false;
        // This is the core of the fast RTTI check. A node's type ID will be within the
        // range of a base type's ID and its highest-ID derived type.
        return (static_cast<uint32_t>(node->typeId) - static_cast<uint32_t>(T::sTypeInfo.typeId) <= static_cast<uint32_t>(T::sTypeInfo.fullDerivedCount));
    }

    template <typename T>
    inline bool node_is_exact(AstNode* node)
    {
        if (node == nullptr)
            return false;
        return node->typeId == T::sTypeInfo.typeId;
    }

    template <typename T>
    inline T* node_dyn_cast(AstNode* node)
    {
        return node_is_a<T>(node) ? static_cast<T*>(node) : nullptr;
    }

    template <typename T>
    inline T* node_dyn_cast_exact(AstNode* node)
    {
        return node_is_exact<T>(node) ? static_cast<T*>(node) : nullptr;
    }

} // namespace Mycelium::Scripting::Lang