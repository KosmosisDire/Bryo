#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace Mycelium::Scripting::Lang
{
    // Forward declarations
    struct AstNode;
    struct ExpressionNode;
    struct StatementNode;
    struct BlockStatementNode;
    struct TypeNameNode;
    struct DeclarationNode;
    struct NamespaceMemberDeclarationNode;
    struct MemberDeclarationNode;
    struct ParameterDeclarationNode;
    struct VariableDeclaratorNode;
    struct ArgumentListNode;
    struct UsingDirectiveNode;
    struct NamespaceDeclarationNode;
    struct LocalVariableDeclarationStatementNode;
    struct ExpressionStatementNode;
    struct UnaryExpressionNode;
    struct TypeParameterNode; // New

    struct SourceLocation
    {
        int line_start = 0;
        int line_end = 0;
        int column_start = 0;
        int column_end = 0;
    };

    struct AstNode
    {
        static inline long long idCounter = 0;
        std::weak_ptr<AstNode> parent;
        std::optional<SourceLocation> location;
        long long id;

        AstNode() : id(idCounter++) {}
        virtual ~AstNode() = default;
    };

    enum class ModifierKind { Public, Private, Protected, Internal, Static, Readonly };

    struct TypeParameterNode : AstNode
    {
        std::string name;
        // Generic constraints (e.g., where T : ISomeInterface) omitted for simple generics
    };

    struct DeclarationNode : AstNode
    {
        std::string name;
        std::vector<ModifierKind> modifiers;
    };

    struct NamespaceMemberDeclarationNode : DeclarationNode {};

    struct UsingDirectiveNode : AstNode
    {
        std::string namespaceName;
    };

    struct NamespaceDeclarationNode : NamespaceMemberDeclarationNode
    {
        std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>> members;
    };

    struct CompilationUnitNode : AstNode
    {
        std::vector<std::shared_ptr<UsingDirectiveNode>> usings;
        std::optional<std::string> fileScopedNamespaceName;
        std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>> members;
    };

    struct TypeDeclarationNode : NamespaceMemberDeclarationNode
    {
        std::vector<std::shared_ptr<TypeParameterNode>> typeParameters; // For class MyClass<T, U>
        std::vector<std::shared_ptr<TypeNameNode>> baseList;
        std::vector<std::shared_ptr<MemberDeclarationNode>> members;
    };

    struct ClassDeclarationNode : TypeDeclarationNode {};
    struct StructDeclarationNode : TypeDeclarationNode {};

    struct MemberDeclarationNode : DeclarationNode
    {
        std::optional<std::shared_ptr<TypeNameNode>> type;
    };

    struct FieldDeclarationNode : MemberDeclarationNode
    {
        std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
    };

    struct MethodDeclarationNode : MemberDeclarationNode
    {
        std::vector<std::shared_ptr<TypeParameterNode>> typeParameters; // For void MyMethod<T>()
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        std::optional<std::shared_ptr<BlockStatementNode>> body;
    };

    struct ConstructorDeclarationNode : MemberDeclarationNode
    {
        // Constructors cannot be generic themselves in C# (the containing type is generic).
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        std::shared_ptr<BlockStatementNode> body;
    };

    struct ParameterDeclarationNode : DeclarationNode
    {
        std::shared_ptr<TypeNameNode> type;
        std::optional<std::shared_ptr<ExpressionNode>> defaultValue;
    };

    struct VariableDeclaratorNode : AstNode
    {
        std::string name;
        std::optional<std::shared_ptr<ExpressionNode>> initializer;
    };

    struct StatementNode : AstNode {};

    struct BlockStatementNode : StatementNode
    {
        std::vector<std::shared_ptr<StatementNode>> statements;
    };

    struct ExpressionStatementNode : StatementNode
    {
        std::shared_ptr<ExpressionNode> expression;
    };

    struct IfStatementNode : StatementNode
    {
        std::shared_ptr<ExpressionNode> condition;
        std::shared_ptr<StatementNode> thenStatement;
        std::optional<std::shared_ptr<StatementNode>> elseStatement;
    };

    struct WhileStatementNode : StatementNode
    {
        std::shared_ptr<ExpressionNode> condition;
        std::shared_ptr<StatementNode> body;
    };

    struct LocalVariableDeclarationStatementNode : StatementNode
    {
        std::shared_ptr<TypeNameNode> type;
        bool isVarDeclaration = false;
        std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
    };

    struct ForStatementNode : StatementNode
    {
        std::optional<std::shared_ptr<LocalVariableDeclarationStatementNode>> declaration;
        std::vector<std::shared_ptr<ExpressionStatementNode>> initializers;
        std::optional<std::shared_ptr<ExpressionNode>> condition;
        std::vector<std::shared_ptr<ExpressionStatementNode>> incrementors;
        std::shared_ptr<StatementNode> body;
    };

    struct ForEachStatementNode : StatementNode
    {
        std::shared_ptr<TypeNameNode> variableType;
        std::string variableName;
        std::shared_ptr<ExpressionNode> collection;
        std::shared_ptr<StatementNode> body;
    };

    struct ReturnStatementNode : StatementNode
    {
        std::optional<std::shared_ptr<ExpressionNode>> expression;
    };

    struct BreakStatementNode : StatementNode {};
    struct ContinueStatementNode : StatementNode {};

    struct ExpressionNode : AstNode {};

    enum class LiteralKind { Integer, String, Boolean, Null, Char, Float };
    struct LiteralExpressionNode : ExpressionNode
    {
        LiteralKind kind;
        std::string value;
    };

    struct IdentifierExpressionNode : ExpressionNode
    {
        std::string name;
        // Note: If this identifier refers to a generic type like 'List' without type arguments
        // (e.g., in typeof(List<>)), this node itself doesn't change.
        // The context (like a TypeOfExpressionNode, not included here) would interpret it.
    };

    enum class UnaryOperatorKind
    {
        LogicalNot, UnaryPlus, UnaryMinus,
        PreIncrement, PostIncrement, PreDecrement, PostDecrement
    };

    struct UnaryExpressionNode : ExpressionNode
    {
        UnaryOperatorKind op;
        std::shared_ptr<ExpressionNode> operand;
    };

    enum class BinaryOperatorKind {
        Add, Subtract, Multiply, Divide, Modulo,
        LogicalAnd, LogicalOr,
        Equals, NotEquals, 
        LessThan, GreaterThan, LessThanOrEqual, GreaterThanOrEqual
        // BitwiseAnd, BitwiseOr, BitwiseXor, LeftShift, RightShift // Optional
    };

    struct BinaryExpressionNode : ExpressionNode {
        std::shared_ptr<ExpressionNode> left;
        BinaryOperatorKind op;
        std::shared_ptr<ExpressionNode> right;
    };

    enum class AssignmentOperator { Assign, AddAssign, SubtractAssign, MultiplyAssign, DivideAssign };
    struct AssignmentExpressionNode : ExpressionNode
    {
        AssignmentOperator op;
        std::shared_ptr<ExpressionNode> target;
        std::shared_ptr<ExpressionNode> source;
    };

    struct MethodCallExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> target;
        // For explicit type arguments in generic method calls, e.g., myObj.GenericMethod<int>()
        std::optional<std::vector<std::shared_ptr<TypeNameNode>>> typeArguments;
        std::shared_ptr<ArgumentListNode> arguments;
    };

    struct MemberAccessExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> target;
        std::string memberName;
        // Note: Accessing a generic type like Namespace.List<int>.StaticMethod()
        // involves the TypeNameNode within the 'target' if it's an IdentifierExpression
        // or another MemberAccessExpression resolving to a generic type.
        // The type arguments are part of the TypeNameNode, not directly here.
    };

    struct ObjectCreationExpressionNode : ExpressionNode
    {
        // TypeNameNode here will handle generic instantiations like new List<int>()
        std::shared_ptr<TypeNameNode> type;
        std::optional<std::shared_ptr<ArgumentListNode>> arguments;
    };

    struct ThisExpressionNode : ExpressionNode {};

    struct TypeNameNode : AstNode
    {
        std::string name; // e.g., "List" for "List<int>", or "T" if referring to a type parameter
        std::vector<std::shared_ptr<TypeNameNode>> typeArguments; // e.g., { "int", "string" } for Dictionary<int, string>
        bool isArray = false;
    };

    struct ArgumentNode : AstNode
    {
        std::optional<std::string> name;
        std::shared_ptr<ExpressionNode> expression;
    };

    struct ArgumentListNode : AstNode
    {
        std::vector<std::shared_ptr<ArgumentNode>> arguments;
    };

} // namespace Mycelium::UI::Lang