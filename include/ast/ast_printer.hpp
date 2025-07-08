#pragma once

#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_rtti.hpp"
#include "common/logger.hpp"
#include <iostream>

using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Scripting::Common;
using namespace Mycelium::Scripting;
using namespace Mycelium;

// A complete visitor to print the entire AST structure.
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

    void print_modifiers(const SizedArray<ModifierKind>& modifiers) {
        if (!modifiers.empty()) {
            LOG_INFO(get_indent() + "Modifiers:", LogCategory::AST);
            indentLevel++;
            for (int i = 0; i < modifiers.size; i++) {
                LOG_INFO(get_indent() + "- " + std::string(to_string(modifiers[i])), LogCategory::AST);
            }
            indentLevel--;
        }
    }

public:
    // --- Base Node Types ---
    
    void visit(AstNode* node) override {
        print_node_header(node, "[Unhandled Node Type]");
    }

    void visit(TokenNode* node) override {
        print_node_header(node, "\"" + std::string(node->text) + "\"");
    }

    void visit(IdentifierNode* node) override {
        print_node_header(node, std::string(node->name));
    }

    // --- Expression Base ---
    
    void visit(ExpressionNode* node) override {
        print_node_header(node, "[Abstract Expression]");
    }

    // --- Expression Implementations ---

    void visit(LiteralExpressionNode* node) override {
        print_node_header(node, std::string(node->token->text) + " (" + std::string(to_string(node->kind)) + ")");
    }

    void visit(IdentifierExpressionNode* node) override {
        print_node_header(node, std::string(node->identifier->name));
    }

    void visit(ParenthesizedExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Expression:", LogCategory::AST);
        indentLevel++;
        node->expression->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(UnaryExpressionNode* node) override {
        std::string op_info = std::string(to_string(node->opKind));
        if (node->isPostfix) op_info += " (postfix)";
        print_node_header(node, op_info);
        indentLevel++;
        LOG_INFO(get_indent() + "Operand:", LogCategory::AST);
        indentLevel++;
        node->operand->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(BinaryExpressionNode* node) override {
        print_node_header(node, "Op: " + std::string(to_string(node->opKind)));
        indentLevel++;
        LOG_INFO(get_indent() + "Left:", LogCategory::AST);
        indentLevel++;
        node->left->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "Right:", LogCategory::AST);
        indentLevel++;
        node->right->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(AssignmentExpressionNode* node) override {
        print_node_header(node, "Op: " + std::string(to_string(node->opKind)));
        indentLevel++;
        LOG_INFO(get_indent() + "Target:", LogCategory::AST);
        indentLevel++;
        node->target->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "Source:", LogCategory::AST);
        indentLevel++;
        node->source->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(CallExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Target:", LogCategory::AST);
        indentLevel++;
        node->target->accept(this);
        indentLevel--;
        if (!node->arguments.empty()) {
            LOG_INFO(get_indent() + "Arguments:", LogCategory::AST);
            indentLevel++;
            for (auto arg : node->arguments) {
                arg->accept(this);
            }
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(MemberAccessExpressionNode* node) override {
        print_node_header(node, "Member: " + std::string(node->member->name));
        indentLevel++;
        LOG_INFO(get_indent() + "Target:", LogCategory::AST);
        indentLevel++;
        node->target->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(NewExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Type:", LogCategory::AST);
        indentLevel++;
        node->type->accept(this);
        indentLevel--;
        if (node->constructorCall) {
            LOG_INFO(get_indent() + "Constructor Call:", LogCategory::AST);
            indentLevel++;
            node->constructorCall->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(ThisExpressionNode* node) override {
        print_node_header(node, "this");
    }

    void visit(CastExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Target Type:", LogCategory::AST);
        indentLevel++;
        node->targetType->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "Expression:", LogCategory::AST);
        indentLevel++;
        node->expression->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(IndexerExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Target:", LogCategory::AST);
        indentLevel++;
        node->target->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "Index:", LogCategory::AST);
        indentLevel++;
        node->index->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(TypeOfExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Type:", LogCategory::AST);
        indentLevel++;
        node->type->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(SizeOfExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Type:", LogCategory::AST);
        indentLevel++;
        node->type->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(WhenExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Expression:", LogCategory::AST);
        indentLevel++;
        node->expression->accept(this);
        indentLevel--;
        if (!node->arms.empty()) {
            LOG_INFO(get_indent() + "Arms:", LogCategory::AST);
            indentLevel++;
            for (auto arm : node->arms) {
                arm->accept(this);
            }
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(ConditionalExpressionNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Condition:", LogCategory::AST);
        indentLevel++;
        node->condition->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "When True:", LogCategory::AST);
        indentLevel++;
        node->whenTrue->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "When False:", LogCategory::AST);
        indentLevel++;
        node->whenFalse->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(RangeExpressionNode* node) override {
        print_node_header(node, "Op: " + std::string(node->rangeOp->text));
        indentLevel++;
        if (node->start) {
            LOG_INFO(get_indent() + "Start:", LogCategory::AST);
            indentLevel++;
            node->start->accept(this);
            indentLevel--;
        }
        if (node->end) {
            LOG_INFO(get_indent() + "End:", LogCategory::AST);
            indentLevel++;
            node->end->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(EnumMemberExpressionNode* node) override {
        print_node_header(node, "Member: " + std::string(node->memberName->name));
    }

    void visit(FieldKeywordExpressionNode* node) override {
        print_node_header(node, "field");
    }

    void visit(ValueKeywordExpressionNode* node) override {
        print_node_header(node, "value");
    }

    // --- Statement Base ---
    
    void visit(StatementNode* node) override {
        print_node_header(node, "[Abstract Statement]");
    }

    // --- Statement Implementations ---

    void visit(EmptyStatementNode* node) override {
        print_node_header(node);
    }

    void visit(BlockStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        for (auto stmt : node->statements) {
            stmt->accept(this);
        }
        indentLevel--;
    }

    void visit(ExpressionStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        node->expression->accept(this);
        indentLevel--;
    }

    void visit(IfStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Condition:", LogCategory::AST);
        indentLevel++;
        node->condition->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "Then:", LogCategory::AST);
        indentLevel++;
        node->thenStatement->accept(this);
        indentLevel--;
        if (node->elseStatement) {
            LOG_INFO(get_indent() + "Else:", LogCategory::AST);
            indentLevel++;
            node->elseStatement->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(WhileStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Condition:", LogCategory::AST);
        indentLevel++;
        node->condition->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "Body:", LogCategory::AST);
        indentLevel++;
        node->body->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(ForStatementNode* node) override {
        print_node_header(node);
        indentLevel++;
        if (node->initializer) {
            LOG_INFO(get_indent() + "Initializer:", LogCategory::AST);
            indentLevel++;
            node->initializer->accept(this);
            indentLevel--;
        }
        if (node->condition) {
            LOG_INFO(get_indent() + "Condition:", LogCategory::AST);
            indentLevel++;
            node->condition->accept(this);
            indentLevel--;
        }
        if (!node->incrementors.empty()) {
            LOG_INFO(get_indent() + "Incrementors:", LogCategory::AST);
            indentLevel++;
            for (auto inc : node->incrementors) {
                inc->accept(this);
            }
            indentLevel--;
        }
        LOG_INFO(get_indent() + "Body:", LogCategory::AST);
        indentLevel++;
        node->body->accept(this);
        indentLevel--;
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

    void visit(BreakStatementNode* node) override {
        print_node_header(node);
    }

    void visit(ContinueStatementNode* node) override {
        print_node_header(node);
    }

    void visit(LocalVariableDeclarationNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Declarators:", LogCategory::AST);
        indentLevel++;
        for (auto decl : node->declarators) {
            decl->accept(this);
        }
        indentLevel--;
        indentLevel--;
    }

    // --- Declaration Base ---
    
    void visit(DeclarationNode* node) override {
        std::string name = node->name ? std::string(node->name->name) : "[unnamed]";
        print_node_header(node, name);
        indentLevel++;
        print_modifiers(node->modifiers);
        indentLevel--;
    }

    // --- Declaration Implementations ---

    void visit(NamespaceDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        if (node->body) {
            LOG_INFO(get_indent() + "Body:", LogCategory::AST);
            indentLevel++;
            node->body->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(UsingDirectiveNode* node) override {
        std::string ns_name = std::string(node->namespaceName->identifier->name);
        print_node_header(node, "using " + ns_name);
    }

    void visit(TypeDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        if (!node->members.empty()) {
            LOG_INFO(get_indent() + "Members:", LogCategory::AST);
            indentLevel++;
            for (auto member : node->members) {
                member->accept(this);
            }
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(InterfaceDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        if (!node->members.empty()) {
            LOG_INFO(get_indent() + "Members:", LogCategory::AST);
            indentLevel++;
            for (auto member : node->members) {
                member->accept(this);
            }
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(EnumDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        if (!node->cases.empty()) {
            LOG_INFO(get_indent() + "Cases:", LogCategory::AST);
            indentLevel++;
            for (auto enumCase : node->cases) {
                enumCase->accept(this);
            }
            indentLevel--;
        }
        if (!node->methods.empty()) {
            LOG_INFO(get_indent() + "Methods:", LogCategory::AST);
            indentLevel++;
            for (auto method : node->methods) {
                method->accept(this);
            }
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(MemberDeclarationNode* node) override {
        std::string name = node->name ? std::string(node->name->name) : "[unnamed]";
        print_node_header(node, name + " [Abstract Member]");
        indentLevel++;
        print_modifiers(node->modifiers);
        indentLevel--;
    }

    void visit(FieldDeclarationNode* node) override {
        std::string field_names;
        for (int i = 0; i < node->names.size; i++) {
            if (i > 0) field_names += ", ";
            field_names += std::string(node->names.values[i]->name);
        }
        print_node_header(node, field_names);
        indentLevel++;
        print_modifiers(node->modifiers);
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
        print_modifiers(node->modifiers);
        if (!node->parameters.empty()) {
            LOG_INFO(get_indent() + "Parameters:", LogCategory::AST);
            indentLevel++;
            for (auto param : node->parameters) {
                param->accept(this);
            }
            indentLevel--;
        }
        if (node->returnType) {
            LOG_INFO(get_indent() + "Return Type:", LogCategory::AST);
            indentLevel++;
            node->returnType->accept(this);
            indentLevel--;
        }
        if (node->body) {
            LOG_INFO(get_indent() + "Body:", LogCategory::AST);
            indentLevel++;
            node->body->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(ParameterNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        LOG_INFO(get_indent() + "Type:", LogCategory::AST);
        indentLevel++;
        node->type->accept(this);
        indentLevel--;
        if (node->defaultValue) {
            LOG_INFO(get_indent() + "Default Value:", LogCategory::AST);
            indentLevel++;
            node->defaultValue->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(VariableDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        if (node->type) {
            LOG_INFO(get_indent() + "Type:", LogCategory::AST);
            indentLevel++;
            node->type->accept(this);
            indentLevel--;
        }
        if (node->initializer) {
            LOG_INFO(get_indent() + "Initializer:", LogCategory::AST);
            indentLevel++;
            node->initializer->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(GenericParameterNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        indentLevel--;
    }

    void visit(PropertyDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        LOG_INFO(get_indent() + "Type:", LogCategory::AST);
        indentLevel++;
        node->type->accept(this);
        indentLevel--;
        if (node->getterExpression) {
            LOG_INFO(get_indent() + "Getter Expression:", LogCategory::AST);
            indentLevel++;
            node->getterExpression->accept(this);
            indentLevel--;
        }
        if (!node->accessors.empty()) {
            LOG_INFO(get_indent() + "Accessors:", LogCategory::AST);
            indentLevel++;
            for (auto accessor : node->accessors) {
                accessor->accept(this);
            }
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(PropertyAccessorNode* node) override {
        std::string accessor_type = std::string(node->accessorKeyword->text);
        print_node_header(node, accessor_type);
        indentLevel++;
        print_modifiers(node->modifiers);
        if (node->expression) {
            LOG_INFO(get_indent() + "Expression:", LogCategory::AST);
            indentLevel++;
            node->expression->accept(this);
            indentLevel--;
        }
        if (node->body) {
            LOG_INFO(get_indent() + "Body:", LogCategory::AST);
            indentLevel++;
            node->body->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(ConstructorDeclarationNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        if (!node->parameters.empty()) {
            LOG_INFO(get_indent() + "Parameters:", LogCategory::AST);
            indentLevel++;
            for (auto param : node->parameters) {
                param->accept(this);
            }
            indentLevel--;
        }
        if (node->body) {
            LOG_INFO(get_indent() + "Body:", LogCategory::AST);
            indentLevel++;
            node->body->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(EnumCaseNode* node) override {
        print_node_header(node, std::string(node->name->name));
        indentLevel++;
        print_modifiers(node->modifiers);
        if (!node->associatedData.empty()) {
            LOG_INFO(get_indent() + "Associated Data:", LogCategory::AST);
            indentLevel++;
            for (auto data : node->associatedData) {
                data->accept(this);
            }
            indentLevel--;
        }
        indentLevel--;
    }

    // --- When Pattern Implementations ---

    void visit(WhenArmNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Pattern:", LogCategory::AST);
        indentLevel++;
        node->pattern->accept(this);
        indentLevel--;
        LOG_INFO(get_indent() + "Result:", LogCategory::AST);
        indentLevel++;
        node->result->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(WhenPatternNode* node) override {
        print_node_header(node, "[Abstract Pattern]");
    }

    void visit(EnumPatternNode* node) override {
        print_node_header(node, "Case: " + std::string(node->enumCase->name));
    }

    void visit(RangePatternNode* node) override {
        print_node_header(node, "Op: " + std::string(node->rangeOp->text));
        indentLevel++;
        if (node->start) {
            LOG_INFO(get_indent() + "Start:", LogCategory::AST);
            indentLevel++;
            node->start->accept(this);
            indentLevel--;
        }
        if (node->end) {
            LOG_INFO(get_indent() + "End:", LogCategory::AST);
            indentLevel++;
            node->end->accept(this);
            indentLevel--;
        }
        indentLevel--;
    }

    void visit(ComparisonPatternNode* node) override {
        print_node_header(node, "Op: " + std::string(node->comparisonOp->text));
        indentLevel++;
        LOG_INFO(get_indent() + "Value:", LogCategory::AST);
        indentLevel++;
        node->value->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(WildcardPatternNode* node) override {
        print_node_header(node, "_");
    }

    void visit(LiteralPatternNode* node) override {
        print_node_header(node);
        indentLevel++;
        node->literal->accept(this);
        indentLevel--;
    }

    // --- Type Name Implementations ---

    void visit(TypeNameNode* node) override {
        print_node_header(node, std::string(node->identifier->name));
    }

    void visit(QualifiedTypeNameNode* node) override {
        print_node_header(node, std::string(node->right->name));
        indentLevel++;
        LOG_INFO(get_indent() + "Left:", LogCategory::AST);
        indentLevel++;
        node->left->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(PointerTypeNameNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Element Type:", LogCategory::AST);
        indentLevel++;
        node->elementType->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(ArrayTypeNameNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Element Type:", LogCategory::AST);
        indentLevel++;
        node->elementType->accept(this);
        indentLevel--;
        indentLevel--;
    }

    void visit(GenericTypeNameNode* node) override {
        print_node_header(node);
        indentLevel++;
        LOG_INFO(get_indent() + "Base Type:", LogCategory::AST);
        indentLevel++;
        node->baseType->accept(this);
        indentLevel--;
        if (!node->arguments.empty()) {
            LOG_INFO(get_indent() + "Type Arguments:", LogCategory::AST);
            indentLevel++;
            for (auto arg : node->arguments) {
                arg->accept(this);
            }
            indentLevel--;
        }
        indentLevel--;
    }

    // --- Root ---
    
    void visit(CompilationUnitNode* node) override {
        print_node_header(node);
        indentLevel++;
        for (auto stmt : node->statements) {
            stmt->accept(this);
        }
        indentLevel--;
    }
};