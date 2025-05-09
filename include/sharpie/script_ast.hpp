#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <iostream> // For std::ostream
#include <sstream>  // For std::stringstream

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
    struct CompilationUnitNode;
    struct TypeDeclarationNode;
    struct ClassDeclarationNode;
    struct StructDeclarationNode;
    struct FieldDeclarationNode;
    struct MethodDeclarationNode;
    struct ConstructorDeclarationNode;
    struct LocalVariableDeclarationStatementNode;
    struct ExpressionStatementNode;
    struct IfStatementNode;
    struct WhileStatementNode;
    struct ForStatementNode;
    struct ForEachStatementNode;
    struct ReturnStatementNode;
    struct BreakStatementNode;
    struct ContinueStatementNode;
    struct LiteralExpressionNode;
    struct IdentifierExpressionNode;
    struct UnaryExpressionNode;
    struct BinaryExpressionNode;
    struct AssignmentExpressionNode;
    struct MethodCallExpressionNode;
    struct MemberAccessExpressionNode;
    struct ObjectCreationExpressionNode;
    struct ThisExpressionNode;
    struct TypeParameterNode;
    struct ArgumentNode;

    // Enums
    enum class ModifierKind { Public, Private, Protected, Internal, Static, Readonly };
    enum class LiteralKind { Integer, String, Boolean, Null, Char, Float };
    enum class UnaryOperatorKind { LogicalNot, UnaryPlus, UnaryMinus, PreIncrement, PostIncrement, PreDecrement, PostDecrement };
    enum class BinaryOperatorKind { Add, Subtract, Multiply, Divide, Modulo, LogicalAnd, LogicalOr, Equals, NotEquals, LessThan, GreaterThan, LessThanOrEqual, GreaterThanOrEqual };
    enum class AssignmentOperator { Assign, AddAssign, SubtractAssign, MultiplyAssign, DivideAssign };

    // Enum string conversion helpers (inline in header)
    inline std::string to_string(ModifierKind kind) {
        switch (kind) {
            case ModifierKind::Public: return "public";
            case ModifierKind::Private: return "private";
            case ModifierKind::Protected: return "protected";
            case ModifierKind::Internal: return "internal";
            case ModifierKind::Static: return "static";
            case ModifierKind::Readonly: return "readonly";
            default: return "unknown_modifier";
        }
    }

    inline std::string modifiers_to_string(const std::vector<ModifierKind>& modifiers) {
        if (modifiers.empty()) return "";
        std::stringstream ss;
        for (size_t i = 0; i < modifiers.size(); ++i) {
            ss << to_string(modifiers[i]) << (i == modifiers.size() - 1 ? "" : " ");
        }
        return ss.str();
    }

    inline std::string to_string(LiteralKind kind) {
        switch (kind) {
            case LiteralKind::Integer: return "Integer";
            case LiteralKind::String: return "String";
            case LiteralKind::Boolean: return "Boolean";
            case LiteralKind::Null: return "Null";
            case LiteralKind::Char: return "Char";
            case LiteralKind::Float: return "Float";
            default: return "unknown_literal";
        }
    }

    inline std::string to_string(UnaryOperatorKind op) {
        switch (op) {
            case UnaryOperatorKind::LogicalNot: return "!";
            case UnaryOperatorKind::UnaryPlus: return "+";
            case UnaryOperatorKind::UnaryMinus: return "-";
            case UnaryOperatorKind::PreIncrement: return "++";
            case UnaryOperatorKind::PostIncrement: return "++";
            case UnaryOperatorKind::PreDecrement: return "--";
            case UnaryOperatorKind::PostDecrement: return "--";
            default: return "unknown_unary_op";
        }
    }

    inline std::string to_string(BinaryOperatorKind op) {
        switch (op) {
            case BinaryOperatorKind::Add: return "+";
            case BinaryOperatorKind::Subtract: return "-";
            case BinaryOperatorKind::Multiply: return "*";
            case BinaryOperatorKind::Divide: return "/";
            case BinaryOperatorKind::Modulo: return "%";
            case BinaryOperatorKind::LogicalAnd: return "&&";
            case BinaryOperatorKind::LogicalOr: return "||";
            case BinaryOperatorKind::Equals: return "==";
            case BinaryOperatorKind::NotEquals: return "!=";
            case BinaryOperatorKind::LessThan: return "<";
            case BinaryOperatorKind::GreaterThan: return ">";
            case BinaryOperatorKind::LessThanOrEqual: return "<=";
            case BinaryOperatorKind::GreaterThanOrEqual: return ">=";
            default: return "unknown_binary_op";
        }
    }

    inline std::string to_string(AssignmentOperator op) {
        switch (op) {
            case AssignmentOperator::Assign: return "=";
            case AssignmentOperator::AddAssign: return "+=";
            case AssignmentOperator::SubtractAssign: return "-=";
            case AssignmentOperator::MultiplyAssign: return "*=";
            case AssignmentOperator::DivideAssign: return "/=";
            default: return "unknown_assign_op";
        }
    }

    struct SourceLocation
    {
        int line_start = 0;
        int line_end = 0;
        int column_start = 0;
        int column_end = 0;

        std::string toString() const {
            std::stringstream ss;
            ss << "[L" << line_start << ":" << column_start
               << "-L" << line_end << ":" << column_end << "]";
            return ss.str();
        }
    };

    struct AstNode
    {
        static inline long long idCounter = 0;
        std::weak_ptr<AstNode> parent;
        std::optional<SourceLocation> location;
        long long id;

        AstNode();
        virtual ~AstNode() = default;
        virtual void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const;
    };

    struct TypeParameterNode : AstNode
    {
        std::string name;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct DeclarationNode : AstNode
    {
        std::string name;
        std::vector<ModifierKind> modifiers;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct NamespaceMemberDeclarationNode : DeclarationNode {};

    struct UsingDirectiveNode : AstNode
    {
        std::string namespaceName;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct NamespaceDeclarationNode : NamespaceMemberDeclarationNode
    {
        std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>> members;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct CompilationUnitNode : AstNode
    {
        std::vector<std::shared_ptr<UsingDirectiveNode>> usings;
        std::optional<std::string> fileScopedNamespaceName;
        std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>> members;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct TypeDeclarationNode : NamespaceMemberDeclarationNode
    {
        std::vector<std::shared_ptr<TypeParameterNode>> typeParameters;
        std::vector<std::shared_ptr<TypeNameNode>> baseList;
        std::vector<std::shared_ptr<MemberDeclarationNode>> members;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ClassDeclarationNode : TypeDeclarationNode {
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };
    struct StructDeclarationNode : TypeDeclarationNode {
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct MemberDeclarationNode : DeclarationNode
    {
        std::optional<std::shared_ptr<TypeNameNode>> type;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct FieldDeclarationNode : MemberDeclarationNode
    {
        std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct MethodDeclarationNode : MemberDeclarationNode
    {
        std::vector<std::shared_ptr<TypeParameterNode>> typeParameters;
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        std::optional<std::shared_ptr<BlockStatementNode>> body;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ConstructorDeclarationNode : MemberDeclarationNode
    {
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        std::shared_ptr<BlockStatementNode> body;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ParameterDeclarationNode : DeclarationNode
    {
        std::shared_ptr<TypeNameNode> type;
        std::optional<std::shared_ptr<ExpressionNode>> defaultValue;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct VariableDeclaratorNode : AstNode
    {
        std::string name;
        std::optional<std::shared_ptr<ExpressionNode>> initializer;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct StatementNode : AstNode {
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct BlockStatementNode : StatementNode
    {
        std::vector<std::shared_ptr<StatementNode>> statements;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ExpressionStatementNode : StatementNode
    {
        std::shared_ptr<ExpressionNode> expression;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct IfStatementNode : StatementNode
    {
        std::shared_ptr<ExpressionNode> condition;
        std::shared_ptr<StatementNode> thenStatement;
        std::optional<std::shared_ptr<StatementNode>> elseStatement;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct WhileStatementNode : StatementNode
    {
        std::shared_ptr<ExpressionNode> condition;
        std::shared_ptr<StatementNode> body;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct LocalVariableDeclarationStatementNode : StatementNode
    {
        std::shared_ptr<TypeNameNode> type;
        bool isVarDeclaration = false;
        std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ForStatementNode : StatementNode
    {
        std::optional<std::shared_ptr<LocalVariableDeclarationStatementNode>> declaration;
        std::vector<std::shared_ptr<ExpressionStatementNode>> initializers;
        std::optional<std::shared_ptr<ExpressionNode>> condition;
        std::vector<std::shared_ptr<ExpressionStatementNode>> incrementors;
        std::shared_ptr<StatementNode> body;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ForEachStatementNode : StatementNode
    {
        std::shared_ptr<TypeNameNode> variableType;
        std::string variableName;
        std::shared_ptr<ExpressionNode> collection;
        std::shared_ptr<StatementNode> body;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ReturnStatementNode : StatementNode
    {
        std::optional<std::shared_ptr<ExpressionNode>> expression;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct BreakStatementNode : StatementNode {
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };
    struct ContinueStatementNode : StatementNode {
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ExpressionNode : AstNode {
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct LiteralExpressionNode : ExpressionNode
    {
        LiteralKind kind;
        std::string value;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct IdentifierExpressionNode : ExpressionNode
    {
        std::string name;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct UnaryExpressionNode : ExpressionNode
    {
        UnaryOperatorKind op;
        std::shared_ptr<ExpressionNode> operand;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct BinaryExpressionNode : ExpressionNode {
        std::shared_ptr<ExpressionNode> left;
        BinaryOperatorKind op;
        std::shared_ptr<ExpressionNode> right;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct AssignmentExpressionNode : ExpressionNode
    {
        AssignmentOperator op;
        std::shared_ptr<ExpressionNode> target;
        std::shared_ptr<ExpressionNode> source;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct MethodCallExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> target;
        std::optional<std::vector<std::shared_ptr<TypeNameNode>>> typeArguments;
        std::shared_ptr<ArgumentListNode> arguments;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct MemberAccessExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> target;
        std::string memberName;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ObjectCreationExpressionNode : ExpressionNode
    {
        std::shared_ptr<TypeNameNode> type;
        std::optional<std::shared_ptr<ArgumentListNode>> arguments;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ThisExpressionNode : ExpressionNode {
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct TypeNameNode : AstNode
    {
        std::string name;
        std::vector<std::shared_ptr<TypeNameNode>> typeArguments;
        bool isArray = false;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ArgumentNode : AstNode
    {
        std::optional<std::string> name;
        std::shared_ptr<ExpressionNode> expression;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

    struct ArgumentListNode : AstNode
    {
        std::vector<std::shared_ptr<ArgumentNode>> arguments;
        void print(std::ostream& out, const std::string& indent = "", bool isLastChild = true) const override;
    };

} // namespace Mycelium::Scripting::Lang