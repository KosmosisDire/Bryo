#pragma once

#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_rtti.hpp"
#include "common/logger.hpp"
#include <iostream>

using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Scripting::Common;
using namespace Mycelium;

// An extended visitor to print a more complex AST structure.
class AstPrinterVisitor : public StructuralVisitor
{
private:
    int indentLevel = 0;

    std::string get_indent() {
        std::string indent;
        for (int i = 0; i < indentLevel; ++i) indent += "    ";
        return indent;
    }

    void print_node_header(AstNode* node, const std::string& extra = "") {
        std::string output = get_indent() + "-> " + g_ordered_type_infos[node->typeId]->name;
        if (!extra.empty()) {
            output += ": " + extra;
        }
        LOG_INFO(output, LogCategory::AST);
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

    void visit(TypeDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        if (!node->members.empty()) {
            LOG_INFO(get_indent() + "Members:", LogCategory::AST);
            indentLevel++;
            for(auto member : node->members) member->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(FieldDeclarationNode* node) override {
        // Handle multiple field names
        std::string field_names;
        for (int i = 0; i < node->names.size; i++) {
            if (i > 0) field_names += ", ";
            field_names += std::string(node->names.values[i]->name);
        }
        print_node_header(node, field_names);
        indentLevel++;
        LOG_INFO(get_indent() + "Type:", LogCategory::AST);
        indentLevel++;
        node->type->accept(this);
        indentLevel--;
        if (node->initializer) {
            LOG_INFO(get_indent() + "Initializer:", LogCategory::AST);
            indentLevel++;
            node->initializer->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(FunctionDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        LOG_INFO(get_indent() + "ReturnType:", LogCategory::AST);
        indentLevel++;
        node->returnType->accept(this);
        indentLevel--;
        if (!node->parameters.empty()) {
            LOG_INFO(get_indent() + "Parameters:", LogCategory::AST);
            indentLevel++;
            for(auto param : node->parameters) param->accept(this);
            indentLevel--;
        }
        if (node->body) {
            LOG_INFO(get_indent() + "Body:", LogCategory::AST);
            node->body->accept(this);
        }
        indentLevel--;
    }

    void visit(ParameterNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        LOG_INFO(get_indent() + "Type:", LogCategory::AST);
        indentLevel++;
        node->type->accept(this);
        indentLevel--;
        indentLevel--;
    }
    
    void visit(VariableDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        LOG_INFO(get_indent() + "Type:", LogCategory::AST);
        indentLevel++;
        node->type->accept(this);
        indentLevel--;
        if (node->initializer) {
            LOG_INFO(get_indent() + "Initializer:", LogCategory::AST);
            node->initializer->accept(this);
        }
        indentLevel--;
    }

    void visit(IfStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Condition:", LogCategory::AST);
        node->condition->accept(this);
        LOG_INFO(get_indent() + "Then:", LogCategory::AST);
        node->thenStatement->accept(this);
        if (node->elseStatement) {
            LOG_INFO(get_indent() + "Else:", LogCategory::AST);
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
        LOG_INFO(get_indent() + "Left:", LogCategory::AST);
        node->left->accept(this);
        LOG_INFO(get_indent() + "Right:", LogCategory::AST);
        node->right->accept(this);
        indentLevel--;
    }

    void visit(AssignmentExpressionNode* node) override {
        print_node_header(node, "Op: " + std::to_string((int)node->opKind));
        indentLevel++;
        LOG_INFO(get_indent() + "Target:", LogCategory::AST);
        node->target->accept(this);
        LOG_INFO(get_indent() + "Source:", LogCategory::AST);
        node->source->accept(this);
        indentLevel--;
    }

    void visit(MemberAccessExpressionNode* node) override {
        print_node_header(node, "Member: " + std::string(node->member->name));
        indentLevel++;
        LOG_INFO(get_indent() + "Target:", LogCategory::AST);
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
        LOG_INFO(get_indent() + "Base:", LogCategory::AST);
        indentLevel++;
        node->baseType->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "Args:", LogCategory::AST);
        indentLevel++;
        for(auto arg : node->arguments) arg->accept(this);
        indentLevel--;
        indentLevel--;
    }

    // Default handler
    void visit(AstNode* node) override {
        print_node_header(node, "[Unhandled in Printer]");
    }
};
