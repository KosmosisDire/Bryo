#pragma once

#include "ast/ast.hpp"
#include <sstream>
#include <string>

namespace Myre {

/**
 * @brief A visitor that traverses an AST and produces a human-readable string representation.
 *
 * Inherits from DefaultVisitor to automatically handle traversal of child nodes.
 * We override visit methods to add indentation and structural braces.
 */
class AstPrinter : public DefaultVisitor {
public:
    /**
     * @brief Prints the given AST node and all its children to a string.
     * @param root The root node of the tree to print.
     * @return A string containing the formatted AST.
     */
    std::string print(Node* root) {
        if (!root) {
            return "[Null AST Node]\n";
        }
        output.str(""); // Clear previous content
        output.clear();
        root->accept(this);
        return output.str();
    }

private:
    std::ostringstream output;
    int indentLevel = 0;

    std::string indent() {
        return std::string(indentLevel * 2, ' ');
    }

    // Prints a single line for a leaf node (no children to indent).
    void leaf(const std::string& name, const std::string& details = "") {
        output << indent() << name << details << "\n";
    }

    // Enters a new indentation level for a branch node.
    void enter(const std::string& name, const std::string& details = "") {
        output << indent() << name << details << " {\n";
        indentLevel++;
    }

    // Leaves the current indentation level.
    void leave() {
        indentLevel--;
        output << indent() << "}\n";
    }

public:
    // --- Override Visitor Methods ---

    // --- Building Blocks & Errors ---
    void visit(Identifier* node) override { leaf("Identifier", " (" + std::string(node->text) + ")"); }
    void visit(ErrorExpression* node) override { leaf("ErrorExpression", " (\"" + std::string(node->message) + "\")"); }
    void visit(ErrorStatement* node) override { leaf("ErrorStatement", " (\"" + std::string(node->message) + "\")"); }
    void visit(ErrorTypeRef* node) override { leaf("ErrorTypeRef", " (\"" + std::string(node->message) + "\")"); }
    void visit(TypedIdentifier* node) override;

    // --- Expressions ---
    void visit(LiteralExpr* node) override;
    void visit(NameExpr* node) override;
    void visit(UnaryExpr* node) override;
    void visit(BinaryExpr* node) override;
    void visit(AssignmentExpr* node) override;
    void visit(ThisExpr* node) override { leaf("ThisExpr"); }

    void visit(ArrayLiteralExpr* node) override { enter("ArrayLiteralExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(CallExpr* node) override { enter("CallExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(MemberAccessExpr* node) override { enter("MemberAccessExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(IndexerExpr* node) override { enter("IndexerExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(CastExpr* node) override { enter("CastExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(NewExpr* node) override { enter("NewExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(LambdaExpr* node) override { enter("LambdaExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(RangeExpr* node) override { enter("RangeExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(ConditionalExpr* node) override { enter("ConditionalExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(TypeOfExpr* node) override { enter("TypeOfExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(SizeOfExpr* node) override { enter("SizeOfExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(IfExpr* node) override { enter("IfExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(MatchExpr* node) override { enter("MatchExpr"); DefaultVisitor::visit(node); leave(); }
    void visit(MatchArm* node) override { enter("MatchArm"); DefaultVisitor::visit(node); leave(); }

    // --- Statements ---
    void visit(Block* node) override { enter("Block"); DefaultVisitor::visit(node); leave(); }
    void visit(ExpressionStmt* node) override { enter("ExpressionStmt"); DefaultVisitor::visit(node); leave(); }
    void visit(ReturnStmt* node) override { enter("ReturnStmt"); DefaultVisitor::visit(node); leave(); }
    void visit(BreakStmt* node) override { leaf("BreakStmt"); }
    void visit(ContinueStmt* node) override { leaf("ContinueStmt"); }
    void visit(WhileStmt* node) override { enter("WhileStmt"); DefaultVisitor::visit(node); leave(); }
    void visit(ForStmt* node) override { enter("ForStmt"); DefaultVisitor::visit(node); leave(); }
    void visit(ForInStmt* node) override { enter("ForInStmt"); DefaultVisitor::visit(node); leave(); }
    void visit(UsingDirective* node) override;
    
    // --- Declarations ---
    void visit(VariableDecl* node) override { enter("VariableDecl"); DefaultVisitor::visit(node); leave(); }
    void visit(MemberVariableDecl* node) override;
    void visit(ParameterDecl* node) override { enter("ParameterDecl"); DefaultVisitor::visit(node); leave(); }
    void visit(GenericParamDecl* node) override;
    void visit(FunctionDecl* node) override;
    void visit(ConstructorDecl* node) override { enter("ConstructorDecl"); DefaultVisitor::visit(node); leave(); }
    void visit(PropertyAccessor* node) override;
    void visit(InheritFunctionDecl* node) override;
    void visit(EnumCaseDecl* node) override;
    void visit(TypeDecl* node) override;
    void visit(NamespaceDecl* node) override;

    // --- Type References ---
    void visit(NamedTypeRef* node) override;
    void visit(ArrayTypeRef* node) override { enter("ArrayTypeRef"); DefaultVisitor::visit(node); leave(); }
    void visit(FunctionTypeRef* node) override { enter("FunctionTypeRef"); DefaultVisitor::visit(node); leave(); }
    void visit(NullableTypeRef* node) override { enter("NullableTypeRef"); DefaultVisitor::visit(node); leave(); }
    void visit(RefTypeRef* node) override { enter("RefTypeRef"); DefaultVisitor::visit(node); leave(); }

    // --- Type Constraints ---
    void visit(BaseTypeConstraint* node) override { enter("BaseTypeConstraint"); DefaultVisitor::visit(node); leave(); }
    void visit(ConstructorConstraint* node) override { enter("ConstructorConstraint"); DefaultVisitor::visit(node); leave(); }

    // --- Patterns ---
    void visit(LiteralPattern* node) override { enter("LiteralPattern"); DefaultVisitor::visit(node); leave(); }
    void visit(BindingPattern* node) override;
    void visit(EnumPattern* node) override;
    void visit(RangePattern* node) override { enter("RangePattern"); DefaultVisitor::visit(node); leave(); }
    void visit(InPattern* node) override { enter("InPattern"); DefaultVisitor::visit(node); leave(); }
    void visit(ComparisonPattern* node) override;

    // --- Root ---
    void visit(CompilationUnit* node) override { enter("CompilationUnit"); DefaultVisitor::visit(node); leave(); }
};

// --- Implementation of methods with more logic ---

inline void AstPrinter::visit(TypedIdentifier* node) {
    enter("TypedIdentifier", " (" + std::string(node->name->text) + ")");
    // We don't call the base visitor for name, since we printed it.
    if (node->type) {
        node->type->accept(this);
    } else {
        leaf("Type: var (inferred)");
    }
    leave();
}

inline void AstPrinter::visit(LiteralExpr* node) {
    leaf("LiteralExpr", " (" + std::string(node->value) + ")");
}

inline void AstPrinter::visit(NameExpr* node) {
    std::string path;
    for (size_t i = 0; i < node->parts.size(); ++i) {
        path += node->parts[i]->text;
        if (i < node->parts.size() - 1) path += "::";
    }
    leaf("NameExpr", " (" + path + ")");
}

inline void AstPrinter::visit(UnaryExpr* node) {
    enter("UnaryExpr", " (Op: " + std::string(to_string(node->op)) + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(BinaryExpr* node) {
    enter("BinaryExpr", " (Op: " + std::string(to_string(node->op)) + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(AssignmentExpr* node) {
    enter("AssignmentExpr", " (Op: " + std::string(to_string(node->op)) + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(UsingDirective* node) {
    if (node->kind == UsingDirective::Kind::Alias) {
        enter("UsingDirective", " (Alias: " + std::string(node->alias->text) + ")");
        if (node->aliasedType) node->aliasedType->accept(this);
        leave();
    } else {
        std::string path;
        for(size_t i = 0; i < node->path.size(); ++i) {
            path += node->path[i]->text;
            if (i < node->path.size() - 1) path += ".";
        }
        leaf("UsingDirective", " (Namespace: " + path + ")");
    }
}

inline void AstPrinter::visit(MemberVariableDecl* node) {
    bool isProperty = node->getter || node->setter;
    enter(isProperty ? "PropertyDecl" : "FieldDecl", " (" + std::string(node->name->text) + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(GenericParamDecl* node) {
    enter("GenericParamDecl", " (" + std::string(node->name->text) + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(FunctionDecl* node) {
    enter("FunctionDecl", " (" + std::string(node->name->text) + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(PropertyAccessor* node) {
    std::string kind = (node->kind == PropertyAccessor::Kind::Get) ? "Get" : "Set";
    enter("PropertyAccessor", " (" + kind + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(InheritFunctionDecl* node) {
    enter("InheritFunctionDecl", " (" + std::string(node->functionName->text) + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(EnumCaseDecl* node) {
    enter("EnumCaseDecl", " (" + std::string(node->name->text) + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(TypeDecl* node) {
    std::string kind_str;
    switch(node->kind) {
        case TypeDecl::Kind::Type: kind_str = "type"; break;
        case TypeDecl::Kind::ValueType: kind_str = "value type"; break;
        case TypeDecl::Kind::RefType: kind_str = "ref type"; break;
        case TypeDecl::Kind::StaticType: kind_str = "static type"; break;
        case TypeDecl::Kind::Enum: kind_str = "enum"; break;
    }
    enter("TypeDecl", " (" + std::string(node->name->text) + ", Kind: " + kind_str + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(NamespaceDecl* node) {
    std::string path;
    for (size_t i = 0; i < node->path.size(); ++i) {
        path += node->path[i]->text;
        if (i < node->path.size() - 1) path += ".";
    }
    enter("NamespaceDecl", " (" + path + (node->isFileScoped ? ", file-scoped" : "") + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(NamedTypeRef* node) {
    std::string path;
    for (size_t i = 0; i < node->path.size(); ++i) {
        path += node->path[i]->text;
        if (i < node->path.size() - 1) path += ".";
    }
    if (node->genericArgs.empty()) {
        leaf("NamedTypeRef", " (" + path + ")");
    } else {
        enter("NamedTypeRef", " (" + path + ")");
        for (auto* arg : node->genericArgs) {
            arg->accept(this);
        }
        leave();
    }
}

inline void AstPrinter::visit(BindingPattern* node) {
    if (node->name) {
        leaf("BindingPattern", " (" + std::string(node->name->text) + ")");
    } else {
        leaf("BindingPattern", " (Wildcard: _)");
    }
}

inline void AstPrinter::visit(EnumPattern* node) {
    std::string path;
    for (size_t i = 0; i < node->path.size(); ++i) {
        path += node->path[i]->text;
        if (i < node->path.size() - 1) path += ".";
    }
    enter("EnumPattern", " (" + path + ")");
    DefaultVisitor::visit(node);
    leave();
}

inline void AstPrinter::visit(ComparisonPattern* node) {
    std::string op_str;
    switch(node->op) {
        case ComparisonPattern::Op::Less: op_str = "<"; break;
        case ComparisonPattern::Op::Greater: op_str = ">"; break;
        case ComparisonPattern::Op::LessEqual: op_str = "<="; break;
        case ComparisonPattern::Op::GreaterEqual: op_str = ">="; break;
    }
    enter("ComparisonPattern", " (Op: " + op_str + ")");
    DefaultVisitor::visit(node);
    leave();
}

} // namespace Myre