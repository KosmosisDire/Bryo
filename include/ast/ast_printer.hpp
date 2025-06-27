#pragma once

#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_rtti.hpp"
#include <iostream>

using namespace Mycelium::Scripting::Lang;

// An extended visitor to print a more complex AST structure.
class AstPrinterVisitor : public StructuralVisitor
{
private:
    int indentLevel = 0;

    void print_indent() {
        for (int i = 0; i < indentLevel; ++i) std::cout << "  ";
    }

    void print_node_header(AstNode* node, const std::string& extra = "") {
        print_indent();
        std::cout << "-> " << g_ordered_type_infos[node->typeId]->name;
        if (!extra.empty()) {
            std::cout << ": " << extra;
        }
        std::cout << std::endl;
    }

public:
    // --- Overridden Visit Methods ---

    void visit(CompilationUnitNode* node) override {
        print_node_header(node);
        indentLevel++;
        for (auto stmt : node->statements) stmt->accept(this);
        indentLevel--;
    }

    void visit(UsingDirectiveNode* node) override {
        std::string ns_name = std::string(node->namespaceName->identifier->name); // Simplified for test
        print_node_header(node, "using " + ns_name);
    }

    void visit(NamespaceDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        node->body->accept(this);
        indentLevel--;
    }

    void visit(ClassDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        if (!node->baseTypes.empty()) {
            print_indent();
            std::cout << "BaseTypes:" << std::endl;
            indentLevel++;
            for(auto base_type : node->baseTypes) base_type->accept(this);
            indentLevel--;
        }
        if (!node->members.empty()) {
            print_indent();
            std::cout << "Members:" << std::endl;
            indentLevel++;
            for(auto member : node->members) member->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(FieldDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_indent(); std::cout << "Type: ";
        node->type->accept(this);
        if (node->initializer) {
            print_indent(); std::cout << "Initializer:" << std::endl;
            indentLevel++;
            node->initializer->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(FunctionDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_indent(); std::cout << "ReturnType: ";
        node->returnType->accept(this);
        if (!node->parameters.empty()) {
            print_indent(); std::cout << "Parameters:" << std::endl;
            indentLevel++;
            for(auto param : node->parameters) param->accept(this);
            indentLevel--;
        }
        if (node->body) {
            print_indent(); std::cout << "Body:" << std::endl;
            node->body->accept(this);
        }
        indentLevel--;
    }

    void visit(ParameterNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_indent(); std::cout << "Type: ";
        node->type->accept(this);
        indentLevel--;
    }
    
    void visit(VariableDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_indent(); std::cout << "Type: ";
        node->type->accept(this);
        if (node->initializer) {
            print_indent(); std::cout << "Initializer:" << std::endl;
            node->initializer->accept(this);
        }
        indentLevel--;
    }

    void visit(IfStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        print_indent(); std::cout << "Condition:" << std::endl;
        node->condition->accept(this);
        print_indent(); std::cout << "Then:" << std::endl;
        node->thenStatement->accept(this);
        if (node->elseStatement) {
            print_indent(); std::cout << "Else:" << std::endl;
            node->elseStatement->accept(this);
        }
        indentLevel--;
    }

    void visit(BlockStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        for(auto stmt : node->statements) stmt->accept(this);
        indentLevel--;
    }

    void visit(ExpressionStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        node->expression->accept(this);
        indentLevel--;
    }

    void visit(ReturnStatementNode* node) override {
        print_node_header(node);
        if (node->expression) {
            indentLevel++;
            node->expression->accept(this);
            indentLevel--;
        }
    }

    void visit(BinaryExpressionNode* node) override {
        print_node_header(node, "Op: " + std::to_string((int)node->opKind));
        indentLevel++;
        print_indent(); std::cout << "Left:" << std::endl;
        node->left->accept(this);
        print_indent(); std::cout << "Right:" << std::endl;
        node->right->accept(this);
        indentLevel--;
    }

    void visit(AssignmentExpressionNode* node) override {
        print_node_header(node, "Op: " + std::to_string((int)node->opKind));
        indentLevel++;
        print_indent(); std::cout << "Target:" << std::endl;
        node->target->accept(this);
        print_indent(); std::cout << "Source:" << std::endl;
        node->source->accept(this);
        indentLevel--;
    }

    void visit(MemberAccessExpressionNode* node) override {
        print_node_header(node, "Member: " + std::string(node->member->name));
        indentLevel++;
        print_indent(); std::cout << "Target:" << std::endl;
        node->target->accept(this);
        indentLevel--;
    }

    void visit(IdentifierExpressionNode* node) override {
        print_node_header(node, std::string(node->identifier->name));
    }
    
    void visit(ThisExpressionNode* node) override {
        print_node_header(node, "this");
    }

    void visit(LiteralExpressionNode* node) override {
        print_node_header(node, std::string(node->token->text));
    }

    void visit(TypeNameNode* node) override {
        print_node_header(node, std::string(node->identifier->name));
        // A real printer would handle generic/qualified types here
    }
    
    void visit(GenericTypeNameNode* node) override {
        print_node_header(node);
        indentLevel++;
        print_indent(); std::cout << "Base: ";
        node->baseType->accept(this);
        print_indent(); std::cout << "Args: ";
        for(auto arg : node->arguments) arg->accept(this);
        indentLevel--;
    }

    // Default handler
    void visit(AstNode* node) override {
        print_node_header(node, "[Unhandled in Printer]");
    }
};
