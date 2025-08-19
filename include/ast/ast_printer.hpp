#pragma once

#include "ast/ast.hpp"
#include "semantic/type.hpp" // For TypePtr and semantic annotations
#include <sstream>
#include <string>
#include <vector>

namespace Myre
{
    /**
     * @brief A visitor that traverses an AST and produces a human-readable string representation,
     * including semantic type annotations for expressions.
     */
    class AstPrinter : public DefaultVisitor
    {
    public:
        /**
         * @brief Prints the given AST node and all its children to a string.
         * @param root The root node of the tree to print.
         * @return A string containing the formatted AST.
         */
        std::string get_string(Node *root)
        {
            if (!root)
            {
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

        std::string indent()
        {
            return std::string(indentLevel * 2, ' ');
        }

        // Helper to get the semantic type annotation string for an expression.
        std::string get_type_annotation(const Node *node)
        {
            // Try to cast the generic Node to a const Expression
            if (const auto *expr = node->as<Expression>())
            {
                if (expr->resolvedType)
                {
                    // If the type is resolved, get its name for printing.
                    return " [Type: " + expr->resolvedType->get_name() + "]";
                }
                else
                {
                    // If the type pointer is null, indicate that.
                    return " [Type: <no type>]";
                }
            }
            // If it's not an expression node, there's no type annotation.
            return "";
        }

        // Prints a single line for a leaf node, automatically adding type info.
        void leaf(const Node *node, const std::string &name, const std::string &details = "")
        {
            output << indent() << name << details << get_type_annotation(node) << "\n";
        }

        // Enters a new indentation level for a branch node, automatically adding type info.
        void enter(const Node *node, const std::string &name, const std::string &details = "")
        {
            output << indent() << name << details << get_type_annotation(node) << " {\n";
            indentLevel++;
        }

        // Leaves the current indentation level.
        void leave(const std::string &message = "")
        {
            indentLevel--;
            output << indent() << "}" << message << "\n";
        }

    public:
        // --- Override Visitor Methods ---

        // --- Building Blocks & Errors ---
        void visit(Identifier *node) override { leaf(node, "Identifier", " (" + std::string(node->text) + ")"); }
        void visit(ErrorExpression *node) override { leaf(node, "ErrorExpression", " (\"" + std::string(node->message) + "\")"); }
        void visit(ErrorStatement *node) override { leaf(node, "ErrorStatement", " (\"" + std::string(node->message) + "\")"); }
        void visit(TypedIdentifier *node) override;

        // --- Expressions ---
        void visit(LiteralExpr *node) override;
        void visit(NameExpr *node) override;
        void visit(UnaryExpr *node) override;
        void visit(BinaryExpr *node) override;
        void visit(AssignmentExpr *node) override;
        void visit(ThisExpr *node) override { leaf(node, "ThisExpr"); }
        void visit(MemberAccessExpr *node) override;
        void visit(ArrayLiteralExpr *node) override { enter(node, "ArrayLiteralExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(CallExpr *node) override { enter(node, "CallExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(IndexerExpr *node) override { enter(node, "IndexerExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(CastExpr *node) override { enter(node, "CastExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(NewExpr *node) override { enter(node, "NewExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(LambdaExpr *node) override { enter(node, "LambdaExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(ConditionalExpr *node) override { enter(node, "ConditionalExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(TypeOfExpr *node) override { enter(node, "TypeOfExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(SizeOfExpr *node) override { enter(node, "SizeOfExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(IfExpr *node) override { enter(node, "IfExpr"); DefaultVisitor::visit(node); leave(); }

        // --- Statements ---
        void visit(Block *node) override { enter(node, "Block"); DefaultVisitor::visit(node); leave(); }
        void visit(ExpressionStmt *node) override { enter(node, "ExpressionStmt"); DefaultVisitor::visit(node); leave(); }
        void visit(ReturnStmt *node) override { enter(node, "ReturnStmt"); DefaultVisitor::visit(node); leave(); }
        void visit(BreakStmt *node) override { leaf(node, "BreakStmt"); }
        void visit(ContinueStmt *node) override { leaf(node, "ContinueStmt"); }
        void visit(WhileStmt *node) override { enter(node, "WhileStmt"); DefaultVisitor::visit(node); leave(); }
        void visit(ForStmt *node) override { enter(node, "ForStmt"); DefaultVisitor::visit(node); leave(); }
        void visit(UsingDirective *node) override;

        // --- Declarations ---
        void visit(VariableDecl *node) override;
        void visit(PropertyDecl *node) override;
        void visit(ParameterDecl *node) override;
        void visit(FunctionDecl *node) override;
        void visit(ConstructorDecl *node) override;
        void visit(PropertyAccessor *node) override;
        void visit(EnumCaseDecl *node) override;
        void visit(TypeDecl *node) override;
        void visit(NamespaceDecl *node) override;

        // --- Type Expressions ---
        void visit(ArrayTypeExpr *node) override { enter(node, "ArrayTypeExpr"); DefaultVisitor::visit(node); leave(); }
        void visit(FunctionTypeExpr *node) override { enter(node, "FunctionTypeExpr"); DefaultVisitor::visit(node); leave(); }

        // --- Root ---
        void visit(CompilationUnit *node) override { enter(node, "CompilationUnit"); DefaultVisitor::visit(node); leave(); }
    };

    // --- Implementation of methods with more logic ---

    inline void AstPrinter::visit(TypedIdentifier *node)
    {
        enter(node, "TypedIdentifier", " (" + std::string(node->name->text) + ")");
        if (node->type)
        {
            node->type->accept(this);
        }
        else
        {
            // Use direct output for simple labels not tied to a node
            output << indent() << "Type: var (inferred)\n";
        }
        leave();
    }

    inline void AstPrinter::visit(LiteralExpr *node)
    {
        leaf(node, "LiteralExpr", " (Kind: " + std::string(to_string(node->kind)) + ", Value: " + std::string(node->value) + ")");
    }

    inline void AstPrinter::visit(NameExpr *node)
    {
        leaf(node, "NameExpr", " (" + node->get_name() + ")");
    }

    inline void AstPrinter::visit(UnaryExpr *node)
    {
        std::string details = " (Op: " + std::string(to_string(node->op));
        if (node->isPostfix)
        {
            details += ", Postfix";
        }
        details += ")";
        enter(node, "UnaryExpr", details);
        DefaultVisitor::visit(node);
        leave();
    }

    inline void AstPrinter::visit(BinaryExpr *node)
    {
        enter(node, "BinaryExpr", " (Op: " + std::string(to_string(node->op)) + ")");
        DefaultVisitor::visit(node);
        leave();
    }

    inline void AstPrinter::visit(AssignmentExpr *node)
    {
        enter(node, "AssignmentExpr", " (Op: " + std::string(to_string(node->op)) + ")");
        DefaultVisitor::visit(node);
        leave();
    }
    
    inline void AstPrinter::visit(MemberAccessExpr *node)
    {
        std::string memberName = node->member ? std::string(node->member->text) : "<unknown>";
        enter(node, "MemberAccessExpr", " (Member: " + memberName + ")");
        node->object->accept(this); // Visit object, but not member since we printed it.
        leave();
    }

    inline void AstPrinter::visit(UsingDirective *node)
    {
        if (node->kind == UsingDirective::Kind::Alias)
        {
            enter(node, "UsingDirective", " (Alias: " + std::string(node->alias->text) + ")");
            if (node->aliasedType)
                node->aliasedType->accept(this);
            leave();
        }
        else
        {
            enter(node, "UsingDirective", " (Namespace)");
            if (node->target)
                node->target->accept(this);
            leave();
        }
    }

    inline void AstPrinter::visit(VariableDecl *node)
    {
        std::string name = (node->variable && node->variable->name) ? std::string(node->variable->name->text) : "<unnamed>";
        enter(node, "VariableDecl", " (" + name + ")" + to_string(node->modifiers));
        DefaultVisitor::visit(node);
        leave();
    }

    inline void AstPrinter::visit(PropertyDecl *node)
    {
        std::string name = (node->variable && node->variable->variable && node->variable->variable->name)
                               ? std::string(node->variable->variable->name->text)
                               : "<invalid>";
        enter(node, "PropertyDecl", " (" + name + ")" + to_string(node->modifiers));
        DefaultVisitor::visit(node);
        leave();
    }
    
    inline void AstPrinter::visit(ParameterDecl *node)
    {
        std::string name = (node->param && node->param->name) ? std::string(node->param->name->text) : "<unnamed>";
        enter(node, "ParameterDecl", " (" + name + ")" + to_string(node->modifiers));
        DefaultVisitor::visit(node);
        leave();
    }

    inline void AstPrinter::visit(FunctionDecl *node)
    {
        enter(node, "FunctionDecl", " (" + std::string(node->name->text) + ")" + to_string(node->modifiers));
        DefaultVisitor::visit(node);
        leave();
    }

    inline void AstPrinter::visit(ConstructorDecl *node)
    {
        enter(node, "ConstructorDecl", to_string(node->modifiers));
        DefaultVisitor::visit(node);
        leave();
    }

    inline void AstPrinter::visit(PropertyAccessor *node)
    {
        std::string kind = (node->kind == PropertyAccessor::Kind::Get) ? "Get" : "Set";
        enter(node, "PropertyAccessor", " (" + kind + ")" + to_string(node->modifiers));
        DefaultVisitor::visit(node);
        leave();
    }

    inline void AstPrinter::visit(EnumCaseDecl *node)
    {
        enter(node, "EnumCaseDecl", " (" + std::string(node->name->text) + ")" + to_string(node->modifiers));
        DefaultVisitor::visit(node);
        leave();
    }

    inline void AstPrinter::visit(TypeDecl *node)
    {
        std::string kind_str;
        switch (node->kind)
        {
        case TypeDecl::Kind::Type: kind_str = "type"; break;
        case TypeDecl::Kind::ValueType: kind_str = "value type"; break;
        case TypeDecl::Kind::RefType: kind_str = "ref type"; break;
        case TypeDecl::Kind::StaticType: kind_str = "static type"; break;
        case TypeDecl::Kind::Enum: kind_str = "enum"; break;
        }
        enter(node, "TypeDecl", " (" + std::string(node->name->text) + ", Kind: " + kind_str + ")" + to_string(node->modifiers));
        DefaultVisitor::visit(node);
        leave(" " + std::string(node->name->text));
    }

    inline void AstPrinter::visit(NamespaceDecl *node)
    {
        std::string extra = node->isFileScoped ? ", file-scoped" : "";
        enter(node, "NamespaceDecl", extra + to_string(node->modifiers));
        
        output << indent() << "Name: {\n";
        indentLevel++;
        if (node->name)
        {
            node->name->accept(this);
        }
        indentLevel--;
        output << indent() << "}\n";

        if (node->body)
        {
            enter(node, "Body");
            for(auto* stmt : *node->body)
            {
                stmt->accept(this);
            }
            leave();
        }
        leave();
    }

} // namespace Myre