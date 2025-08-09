#pragma once

#include <cstdint>
#include <string>
#include <cstring> // for memcpy
#include <functional> // for std::function in TypeSafeVisitor
#include "ast_rtti.hpp"
#include "ast_allocator.hpp"
#include "common/token.hpp"

namespace Myre
{
    // --- Forward Declarations ---
    // This comprehensive list allows any node to reference any other node.
    struct AstNode;
    struct ErrorNode;
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
    struct ForInStatementNode;
    struct ReturnStatementNode;
    struct BreakStatementNode;
    struct ContinueStatementNode;
    struct EmptyStatementNode;

    // Declarations
    struct NamespaceDeclarationNode;
    struct UsingDirectiveNode;
    struct TypeDeclarationNode;
    struct InterfaceDeclarationNode;
    struct EnumDeclarationNode;
    struct MemberDeclarationNode;
    struct FunctionDeclarationNode;
    struct ParameterNode;
    struct VariableDeclarationNode;
    struct CompilationUnitNode;

    // Types
    struct QualifiedNameNode;
    struct TypeNameNode;
    struct ArrayTypeNameNode;
    struct GenericTypeNameNode;
    struct GenericParameterNode;

    // Match Expressions and Patterns
    struct MatchExpressionNode;
    struct MatchArmNode;
    struct MatchPatternNode;
    struct EnumPatternNode;
    struct RangePatternNode;
    struct ComparisonPatternNode;
    struct WildcardPatternNode;
    struct LiteralPatternNode;

    // Properties
    struct PropertyDeclarationNode;
    struct PropertyAccessorNode;

    // Constructor
    struct ConstructorDeclarationNode;

    // Enum Cases
    struct EnumCaseNode;

    // Additional Expressions
    struct ConditionalExpressionNode;
    struct RangeExpressionNode;
    struct FieldKeywordExpressionNode;
    struct ValueKeywordExpressionNode;
    const char* get_type_name_from_id(uint8_t typeId);

    
    // The base class for all AST traversal and analysis passes.
    // Now includes type-safe error handling capabilities.
    class StructuralVisitor
    {

    public:
        virtual ~StructuralVisitor() = default;

        // The default visit function, which all others will call.
        void visit(AstNode* node) {};

        // One visit method for each concrete AND abstract node type.
        virtual void visit(ErrorNode* node);
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
        virtual void visit(MatchExpressionNode* node);
        virtual void visit(ConditionalExpressionNode* node);
        virtual void visit(RangeExpressionNode* node);
        virtual void visit(FieldKeywordExpressionNode* node);
        virtual void visit(ValueKeywordExpressionNode* node);
        
        // Statements
        virtual void visit(StatementNode* node);
        virtual void visit(BlockStatementNode* node);
        virtual void visit(ExpressionStatementNode* node);
        virtual void visit(IfStatementNode* node);
        virtual void visit(WhileStatementNode* node);
        virtual void visit(ForStatementNode* node);
        virtual void visit(ForInStatementNode* node);
        virtual void visit(ReturnStatementNode* node);
        virtual void visit(BreakStatementNode* node);
        virtual void visit(ContinueStatementNode* node);
        virtual void visit(EmptyStatementNode* node);
        // Declarations
        virtual void visit(DeclarationNode* node);
        virtual void visit(NamespaceDeclarationNode* node);
        virtual void visit(UsingDirectiveNode* node);
        virtual void visit(TypeDeclarationNode* node);
        virtual void visit(InterfaceDeclarationNode* node);
        virtual void visit(EnumDeclarationNode* node);
        virtual void visit(MemberDeclarationNode* node);
        virtual void visit(FunctionDeclarationNode* node);
        virtual void visit(ParameterNode* node);
        virtual void visit(VariableDeclarationNode* node);
        virtual void visit(GenericParameterNode* node);
        virtual void visit(PropertyDeclarationNode* node);
        virtual void visit(PropertyAccessorNode* node);
        virtual void visit(ConstructorDeclarationNode* node);
        virtual void visit(EnumCaseNode* node);
        
        // Match Patterns
        virtual void visit(MatchArmNode* node);
        virtual void visit(MatchPatternNode* node);
        virtual void visit(EnumPatternNode* node);
        virtual void visit(RangePatternNode* node);
        virtual void visit(ComparisonPatternNode* node);
        virtual void visit(WildcardPatternNode* node);
        virtual void visit(LiteralPatternNode* node);
        
        // Types
        virtual void visit(QualifiedNameNode* node);
        virtual void visit(TypeNameNode* node);
        virtual void visit(ArrayTypeNameNode* node);
        virtual void visit(GenericTypeNameNode* node);
        
        // Root
        virtual void visit(CompilationUnitNode* node);
    };

    // A non-owning array view for collections of AST nodes.
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


    // --- Base AST Node ---
    struct AstNode
    {
        AST_ROOT_TYPE(AstNode) // Root node has no base type

        uint8_t typeId;
        SourceRange location;

        // Must be implemented in ast.cpp
        void init_with_type_id(uint8_t id);
        void accept(StructuralVisitor* visitor);

        // RTTI functions defined at the end of this file
        template <typename T> bool is_a() { return node_is<T>(this); }
        template <typename T> T* as() { return node_cast<T>(this); }
        const char* node_type_name() { return get_type_name_from_id(typeId); }
    };

    struct ErrorNode : AstNode {
        AST_TYPE(ErrorNode, AstNode)

        static ErrorNode* create(const SourceRange location, AstAllocator& allocator) {
            auto* node = allocator.alloc<ErrorNode>();
            node->location = location;
            return node;
        }
    }; AST_DECL_IMPL(ErrorNode, AstNode)


    // --- Node Hierarchy ---

    struct TokenNode : AstNode
    {
        AST_TYPE(TokenNode, AstNode)
        std::string_view text;
    }; AST_DECL_IMPL(TokenNode, AstNode)

    struct IdentifierNode : AstNode
    {
        AST_TYPE(IdentifierNode, AstNode)
        std::string_view name;
    }; AST_DECL_IMPL(IdentifierNode, AstNode)

    // --- Expressions ---
    struct ExpressionNode : AstNode
    {
        AST_TYPE(ExpressionNode, AstNode)
    }; AST_DECL_IMPL(ExpressionNode, AstNode)

    struct LiteralExpressionNode : ExpressionNode
    {
        AST_TYPE(LiteralExpressionNode, ExpressionNode)
        LiteralKind kind;
        TokenNode* token; // Contains the raw text
    }; AST_DECL_IMPL(LiteralExpressionNode, ExpressionNode)

    struct IdentifierExpressionNode : ExpressionNode
    {
        AST_TYPE(IdentifierExpressionNode, ExpressionNode)
        IdentifierNode* identifier;
    }; AST_DECL_IMPL(IdentifierExpressionNode, ExpressionNode)

    struct ParenthesizedExpressionNode : ExpressionNode
    {
        AST_TYPE(ParenthesizedExpressionNode, ExpressionNode)
        TokenNode* openParen;
        ExpressionNode* expression;
        TokenNode* closeParen;
    }; AST_DECL_IMPL(ParenthesizedExpressionNode, ExpressionNode)

    struct UnaryExpressionNode : ExpressionNode
    {
        AST_TYPE(UnaryExpressionNode, ExpressionNode)
        UnaryOperatorKind opKind;
        TokenNode* operatorToken;
        ExpressionNode* operand;
        bool isPostfix;
    }; AST_DECL_IMPL(UnaryExpressionNode, ExpressionNode)

    struct BinaryExpressionNode : ExpressionNode
    {
        AST_TYPE(BinaryExpressionNode, ExpressionNode)
        AstNode* left;        // Can be ExpressionNode* or ErrorNode*
        BinaryOperatorKind opKind;
        TokenNode* operatorToken;
        AstNode* right;       // Can be ExpressionNode* or ErrorNode*
    }; AST_DECL_IMPL(BinaryExpressionNode, ExpressionNode)

    struct AssignmentExpressionNode : ExpressionNode
    {
        AST_TYPE(AssignmentExpressionNode, ExpressionNode)
        ExpressionNode* target;
        AssignmentOperatorKind opKind;
        TokenNode* operatorToken;
        ExpressionNode* source;
    }; AST_DECL_IMPL(AssignmentExpressionNode, ExpressionNode)

    struct CallExpressionNode : ExpressionNode
    {
        AST_TYPE(CallExpressionNode, ExpressionNode)
        ExpressionNode* target;
        TokenNode* openParen;
        SizedArray<AstNode*> arguments;  // Can contain ExpressionNodes or ErrorNodes
        SizedArray<TokenNode*> commas;
        TokenNode* closeParen;
    }; AST_DECL_IMPL(CallExpressionNode, ExpressionNode)

    struct MemberAccessExpressionNode : ExpressionNode
    {
        AST_TYPE(MemberAccessExpressionNode, ExpressionNode)
        ExpressionNode* target;
        TokenNode* dotToken;
        IdentifierNode* member;
    }; AST_DECL_IMPL(MemberAccessExpressionNode, ExpressionNode)

    struct NewExpressionNode : ExpressionNode
    {
        AST_TYPE(NewExpressionNode, ExpressionNode)
        TokenNode* newKeyword;
        TypeNameNode* type;
        CallExpressionNode* constructorCall; // Optional, can be null
    }; AST_DECL_IMPL(NewExpressionNode, ExpressionNode)

    struct ThisExpressionNode : ExpressionNode
    {
        AST_TYPE(ThisExpressionNode, ExpressionNode)
        TokenNode* thisKeyword;
    }; AST_DECL_IMPL(ThisExpressionNode, ExpressionNode)

    struct CastExpressionNode : ExpressionNode
    {
        AST_TYPE(CastExpressionNode, ExpressionNode)
        TokenNode* openParen;
        TypeNameNode* targetType;
        TokenNode* closeParen;
        ExpressionNode* expression;
    }; AST_DECL_IMPL(CastExpressionNode, ExpressionNode)

    struct IndexerExpressionNode : ExpressionNode
    {
        AST_TYPE(IndexerExpressionNode, ExpressionNode)
        ExpressionNode* target;
        TokenNode* openBracket;
        ExpressionNode* index;
        TokenNode* closeBracket;
    }; AST_DECL_IMPL(IndexerExpressionNode, ExpressionNode)

    struct TypeOfExpressionNode : ExpressionNode
    {
        AST_TYPE(TypeOfExpressionNode, ExpressionNode)
        TokenNode* typeOfKeyword;
        TokenNode* openParen;
        TypeNameNode* type;
        TokenNode* closeParen;
    }; AST_DECL_IMPL(TypeOfExpressionNode, ExpressionNode)

    struct SizeOfExpressionNode : ExpressionNode
    {
        AST_TYPE(SizeOfExpressionNode, ExpressionNode)
        TokenNode* sizeOfKeyword;
        TokenNode* openParen;
        TypeNameNode* type;
        TokenNode* closeParen;
    }; AST_DECL_IMPL(SizeOfExpressionNode, ExpressionNode)

    struct MatchExpressionNode : ExpressionNode
    {
        AST_TYPE(MatchExpressionNode, ExpressionNode)
        TokenNode* matchKeyword;
        TokenNode* openParen;
        ExpressionNode* expression;
        TokenNode* closeParen;
        TokenNode* openBrace;
        SizedArray<MatchArmNode*> arms;
        TokenNode* closeBrace;
    }; AST_DECL_IMPL(MatchExpressionNode, ExpressionNode)

    struct ConditionalExpressionNode : ExpressionNode
    {
        AST_TYPE(ConditionalExpressionNode, ExpressionNode)
        ExpressionNode* condition;
        TokenNode* question;
        ExpressionNode* whenTrue;
        TokenNode* colon;
        ExpressionNode* whenFalse;
    }; AST_DECL_IMPL(ConditionalExpressionNode, ExpressionNode)

    struct RangeExpressionNode : ExpressionNode
    {
        AST_TYPE(RangeExpressionNode, ExpressionNode)
        ExpressionNode* start;
        TokenNode* rangeOp; // .. or ..=
        ExpressionNode* end;
        TokenNode* byKeyword; // optional
        ExpressionNode* stepExpression; // var i or Type var or just an identifier, optional
    }; AST_DECL_IMPL(RangeExpressionNode, ExpressionNode)

    struct FieldKeywordExpressionNode : ExpressionNode
    {
        AST_TYPE(FieldKeywordExpressionNode, ExpressionNode)
        TokenNode* fieldKeyword;
    }; AST_DECL_IMPL(FieldKeywordExpressionNode, ExpressionNode)

    struct ValueKeywordExpressionNode : ExpressionNode
    {
        AST_TYPE(ValueKeywordExpressionNode, ExpressionNode)
        TokenNode* valueKeyword;
    }; AST_DECL_IMPL(ValueKeywordExpressionNode, ExpressionNode)

    // --- Statements ---
    struct StatementNode : AstNode
    { 
        AST_TYPE(StatementNode, AstNode)
    }; AST_DECL_IMPL(StatementNode, AstNode)

    struct EmptyStatementNode : StatementNode
    {
        AST_TYPE(EmptyStatementNode, StatementNode)
        TokenNode* semicolon;
    }; AST_DECL_IMPL(EmptyStatementNode, StatementNode)

    struct BlockStatementNode : StatementNode
    {
        AST_TYPE(BlockStatementNode, StatementNode)
        TokenNode* openBrace;
        // A block can contain both statements and local declarations (including ErrorNodes)
        SizedArray<AstNode*> statements;
        TokenNode* closeBrace;
    }; AST_DECL_IMPL(BlockStatementNode, StatementNode)

    struct ExpressionStatementNode : StatementNode
    {
        AST_TYPE(ExpressionStatementNode, StatementNode)
        AstNode* expression;  // Can be ExpressionNode* or ErrorNode*
        TokenNode* semicolon;
    }; AST_DECL_IMPL(ExpressionStatementNode, StatementNode)

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
    }; AST_DECL_IMPL(IfStatementNode, StatementNode)

    struct WhileStatementNode : StatementNode
    {
        AST_TYPE(WhileStatementNode, StatementNode)
        TokenNode* whileKeyword;
        TokenNode* openParen;
        ExpressionNode* condition;
        TokenNode* closeParen;
        StatementNode* body;
    }; AST_DECL_IMPL(WhileStatementNode, StatementNode)

    struct ForStatementNode : StatementNode
    {
        AST_TYPE(ForStatementNode, StatementNode)
        TokenNode* forKeyword;
        TokenNode* openParen;
        StatementNode* initializer; // VariableDeclarationNode or ExpressionStatementNode
        ExpressionNode* condition;
        TokenNode* firstSemicolon;
        SizedArray<ExpressionNode*> incrementors;
        TokenNode* secondSemicolon;
        TokenNode* closeParen;
        StatementNode* body;
    }; AST_DECL_IMPL(ForStatementNode, StatementNode)

    struct ForInStatementNode : StatementNode
    {
        AST_TYPE(ForInStatementNode, StatementNode)
        TokenNode* forKeyword;
        TokenNode* openParen;
        StatementNode* mainVariable;  // var i or Type var or just an identifier
        TokenNode* inKeyword;
        ExpressionNode* iterable;  // 0..10 or collection
        TokenNode* atKeyword; // optional
        StatementNode* indexVariable;  // var i or Type var or just an identifier, optional
        TokenNode* closeParen;
        StatementNode* body;
    }; AST_DECL_IMPL(ForInStatementNode, StatementNode)

    struct ReturnStatementNode : StatementNode
    {
        AST_TYPE(ReturnStatementNode, StatementNode)
        TokenNode* returnKeyword;
        ExpressionNode* expression; // Optional, can be null
        TokenNode* semicolon;
    }; AST_DECL_IMPL(ReturnStatementNode, StatementNode)

    struct BreakStatementNode : StatementNode
    {
        AST_TYPE(BreakStatementNode, StatementNode)
        TokenNode* breakKeyword;
        TokenNode* semicolon;
    }; AST_DECL_IMPL(BreakStatementNode, StatementNode)

    struct ContinueStatementNode : StatementNode
    {
        AST_TYPE(ContinueStatementNode, StatementNode)
        TokenNode* continueKeyword;
        TokenNode* semicolon;
    }; AST_DECL_IMPL(ContinueStatementNode, StatementNode)

    // --- Type Names ---

    struct QualifiedNameNode : AstNode
    {
        AST_TYPE(QualifiedNameNode, AstNode)
        SizedArray<IdentifierNode*> identifiers;

        std::string get_full_name() const
        {
            std::string full_name;
            for (const auto& id : identifiers)
            {
                if (!full_name.empty())
                    full_name += ".";
                full_name += id->name;
            }
            return full_name;
        }

        std::string_view get_name() const
        {
            if (identifiers.empty())
                return "";
            return identifiers.back()->name;
        }
    }; AST_DECL_IMPL(QualifiedNameNode, AstNode)

    struct TypeNameNode : AstNode
    {
        AST_TYPE(TypeNameNode, AstNode)
        QualifiedNameNode* name;

        std::string get_full_name() const
        {
            if (name)
                return name->get_full_name();
            return "";
        }
        std::string_view get_name() const
        {
            if (name)
                return name->get_name();
            return "";
        }
    }; AST_DECL_IMPL(TypeNameNode, AstNode)

    struct ArrayTypeNameNode : TypeNameNode
    {
        AST_TYPE(ArrayTypeNameNode, TypeNameNode)
        TypeNameNode* elementType;
        TokenNode* openBracket;
        TokenNode* closeBracket;
    }; AST_DECL_IMPL(ArrayTypeNameNode, TypeNameNode)

    struct GenericTypeNameNode : TypeNameNode
    {
        AST_TYPE(GenericTypeNameNode, TypeNameNode)
        TypeNameNode* baseType;
        TokenNode* openAngle;
        SizedArray<AstNode*> arguments;  // Can contain TypeNameNodes or ErrorNodes
        SizedArray<TokenNode*> commas;
        TokenNode* closeAngle;
    }; AST_DECL_IMPL(GenericTypeNameNode, TypeNameNode)

    // --- Declarations ---
    struct DeclarationNode : StatementNode
    {
        AST_TYPE(DeclarationNode, StatementNode)
        SizedArray<ModifierKind> modifiers;
    }; AST_DECL_IMPL(DeclarationNode, StatementNode)

    struct ParameterNode : DeclarationNode
    {
        AST_TYPE(ParameterNode, DeclarationNode)
        IdentifierNode* name;
        TypeNameNode* type;
        TokenNode* equalsToken; // Optional, can be null
        ExpressionNode* defaultValue; // Optional, can be null
    }; AST_DECL_IMPL(ParameterNode, DeclarationNode)

    struct VariableDeclarationNode : DeclarationNode
    {
        AST_TYPE(VariableDeclarationNode, DeclarationNode)
        TokenNode* varKeyword; // Optional, can be null
        TypeNameNode* type; // Optional, can be null
        // either the var keyword or type must be present
        SizedArray<IdentifierNode*> names; // if only one name then use name otherwise use names
        TokenNode* equalsToken; // Optional, can be null
        ExpressionNode* initializer; // Optional, can be null
        TokenNode* semicolon;

        IdentifierNode* first_name() const {
            return names.size > 0 ? names[0] : nullptr;
        }
    }; AST_DECL_IMPL(VariableDeclarationNode, DeclarationNode)

    struct MemberDeclarationNode : DeclarationNode
    {
        AST_TYPE(MemberDeclarationNode, DeclarationNode)
        IdentifierNode* name;
    }; AST_DECL_IMPL(MemberDeclarationNode, DeclarationNode)

    struct GenericParameterNode : DeclarationNode
    {
        AST_TYPE(GenericParameterNode, DeclarationNode)
        IdentifierNode* name;
    }; AST_DECL_IMPL(GenericParameterNode, DeclarationNode)

    struct FunctionDeclarationNode : MemberDeclarationNode
    {
        AST_TYPE(FunctionDeclarationNode, MemberDeclarationNode)
        TokenNode* fnKeyword;
        IdentifierNode* name;
        TokenNode* openParen;
        SizedArray<AstNode*> parameters;  // Can contain ParameterNodes or ErrorNodes
        TokenNode* closeParen;
        // modifiers inherited from DeclarationNode
        TokenNode* arrow; // optional -> for return type
        TypeNameNode* returnType; // optional, after arrow
        BlockStatementNode* body; // can be null for abstract
        TokenNode* semicolon; // for abstract functions
    }; AST_DECL_IMPL(FunctionDeclarationNode, MemberDeclarationNode)

    struct TypeDeclarationNode : DeclarationNode
    {
        AST_TYPE(TypeDeclarationNode, DeclarationNode)
        TokenNode* typeKeyword; // always "type"
        IdentifierNode* name;
        TokenNode* openBrace;
        SizedArray<AstNode*> members;  // Can contain MemberDeclarationNodes or ErrorNodes
        TokenNode* closeBrace;
    }; AST_DECL_IMPL(TypeDeclarationNode, DeclarationNode)

    struct InterfaceDeclarationNode : DeclarationNode
    {
        AST_TYPE(InterfaceDeclarationNode, DeclarationNode)
        TokenNode* interfaceKeyword;
        IdentifierNode* name;
        TokenNode* openBrace;
        SizedArray<MemberDeclarationNode*> members;
        TokenNode* closeBrace;
    }; AST_DECL_IMPL(InterfaceDeclarationNode, DeclarationNode)

    struct EnumDeclarationNode : DeclarationNode
    {
        AST_TYPE(EnumDeclarationNode, DeclarationNode)
        TokenNode* enumKeyword;
        IdentifierNode* name;
        TokenNode* openBrace;
        SizedArray<EnumCaseNode*> cases;
        SizedArray<FunctionDeclarationNode*> methods; // enums can have methods
        TokenNode* closeBrace;
    }; AST_DECL_IMPL(EnumDeclarationNode, DeclarationNode)

    struct UsingDirectiveNode : StatementNode
    {
        AST_TYPE(UsingDirectiveNode, StatementNode)
        TokenNode* usingKeyword;
        QualifiedNameNode* namespaceName;
        TokenNode* semicolon;
    }; AST_DECL_IMPL(UsingDirectiveNode, StatementNode)

    struct NamespaceDeclarationNode : DeclarationNode
    {
        AST_TYPE(NamespaceDeclarationNode, DeclarationNode)
        TokenNode* namespaceKeyword;
        QualifiedNameNode* name;
        BlockStatementNode* body; // File-scoped namespaces might not have braces
    }; AST_DECL_IMPL(NamespaceDeclarationNode, DeclarationNode)

    // The root of a parsed file
    struct CompilationUnitNode : AstNode
    {
        AST_TYPE(CompilationUnitNode, AstNode)
        SizedArray<AstNode*> statements; // Can contain usings, namespace, function, class decls, and ErrorNodes
    }; AST_DECL_IMPL(CompilationUnitNode, AstNode)

    // --- Match Patterns ---
    struct MatchArmNode : AstNode
    {
        AST_TYPE(MatchArmNode, AstNode)
        MatchPatternNode* pattern;
        TokenNode* arrow; // =>
        ExpressionNode* result; // can be expression or block
        TokenNode* comma; // optional trailing comma
    }; AST_DECL_IMPL(MatchArmNode, AstNode)

    struct MatchPatternNode : AstNode
    { 
        AST_TYPE(MatchPatternNode, AstNode)
    }; AST_DECL_IMPL(MatchPatternNode, AstNode)

    struct EnumPatternNode : MatchPatternNode
    {
        AST_TYPE(EnumPatternNode, MatchPatternNode)
        TokenNode* dot;
        IdentifierNode* enumCase;
    }; AST_DECL_IMPL(EnumPatternNode, MatchPatternNode)

    struct RangePatternNode : MatchPatternNode
    {
        AST_TYPE(RangePatternNode, MatchPatternNode)
        ExpressionNode* start; // optional for open ranges
        TokenNode* rangeOp; // .. or ..=
        ExpressionNode* end; // optional for open ranges
    }; AST_DECL_IMPL(RangePatternNode, MatchPatternNode)

    struct ComparisonPatternNode : MatchPatternNode
    {
        AST_TYPE(ComparisonPatternNode, MatchPatternNode)
        TokenNode* comparisonOp; // <=, >=, <, >
        ExpressionNode* value;
    }; AST_DECL_IMPL(ComparisonPatternNode, MatchPatternNode)

    struct WildcardPatternNode : MatchPatternNode
    {
        AST_TYPE(WildcardPatternNode, MatchPatternNode)
        TokenNode* underscore;
    }; AST_DECL_IMPL(WildcardPatternNode, MatchPatternNode)

    struct LiteralPatternNode : MatchPatternNode
    {
        AST_TYPE(LiteralPatternNode, MatchPatternNode)
        LiteralExpressionNode* literal;
    }; AST_DECL_IMPL(LiteralPatternNode, MatchPatternNode)

    // --- Properties ---
    struct PropertyDeclarationNode : MemberDeclarationNode
    {
        AST_TYPE(PropertyDeclarationNode, MemberDeclarationNode)
        // name inherited from DeclarationNode
        TokenNode* colon;
        TokenNode* propKeyword; // optional for full syntax
        TypeNameNode* type;
        TokenNode* arrow; // optional for expression-bodied getter
        ExpressionNode* getterExpression; // for => syntax
        TokenNode* openBrace; // optional for accessor block
        SizedArray<PropertyAccessorNode*> accessors;
        TokenNode* closeBrace; // optional
    }; AST_DECL_IMPL(PropertyDeclarationNode, MemberDeclarationNode)

    struct PropertyAccessorNode : AstNode
    {
        AST_TYPE(PropertyAccessorNode, AstNode)
        SizedArray<ModifierKind> modifiers; // public, protected, etc.
        TokenNode* accessorKeyword; // "get" or "set"
        TokenNode* arrow; // optional =>
        ExpressionNode* expression; // for => field
        BlockStatementNode* body; // for { } syntax
    }; AST_DECL_IMPL(PropertyAccessorNode, AstNode)

    // --- Constructor ---
    struct ConstructorDeclarationNode : MemberDeclarationNode
    {
        AST_TYPE(ConstructorDeclarationNode, MemberDeclarationNode)
        TokenNode* newKeyword;
        TokenNode* openParen;
        SizedArray<ParameterNode*> parameters;
        TokenNode* closeParen;
        BlockStatementNode* body;
    }; AST_DECL_IMPL(ConstructorDeclarationNode, MemberDeclarationNode)

    // --- Enum Cases ---
    struct EnumCaseNode : MemberDeclarationNode
    {
        AST_TYPE(EnumCaseNode, MemberDeclarationNode)
        TokenNode* caseKeyword;
        // name inherited from DeclarationNode
        TokenNode* openParen; // optional
        SizedArray<ParameterNode*> associatedData; // for Square(x, y, w, h)
        TokenNode* closeParen; // optional
    }; AST_DECL_IMPL(EnumCaseNode, MemberDeclarationNode)



    
    template <typename T>
    inline bool node_is(AstNode* node)
    {
        if (node == nullptr)
            return false;
        // This is the core of the fast RTTI check. A node's type ID will be within the
        // range of a base type's ID and its highest-ID derived type.
        return (static_cast<uint32_t>(node->typeId) - static_cast<uint32_t>(T::sTypeInfo.typeId) <= static_cast<uint32_t>(T::sTypeInfo.fullDerivedCount));
    }

    template <typename T>
    inline T* node_cast(AstNode* node)
    {
        return node_is<T>(node) ? static_cast<T*>(node) : nullptr;
    }

    inline const char* get_type_name_from_id(uint8_t type_id)
    {
        assert(!g_type_infos.empty() && "RTTI system not initialized. Call AstTypeInfo::initialize() first.");

        if (type_id < g_type_infos.size()) {
            return g_type_infos[type_id]->name;
        }
        return "UnknownType";
    }

    inline const char* get_node_type_name(const AstNode* node) {
        if (!node) {
            return "NullNode";
        }
        return get_type_name_from_id(node->typeId);
    }
    

} // namespace Myre