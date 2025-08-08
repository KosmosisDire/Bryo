#pragma once

#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_rtti.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <sstream>

using namespace Myre;

class AstPrinter : public StructuralVisitor
{
private:
    int indentLevel = 0;
    std::ostringstream output;

    std::string get_indent() {
        std::string indent;
        for (int i = 0; i < indentLevel; ++i) indent += "  ";
        return indent;
    }

    void print_line(const std::string& text) {
        std::string line = get_indent() + text;
        LOG_INFO(line, LogCategory::AST);
    }

    void print_inline(const std::string& text) {
        output << text;
    }

    void print_node_open(const std::string& nodeType) {
        print_inline(nodeType + " {");
    }

    void print_node_close() {
        print_line("}");
    }

    void print_collection(const std::string& name, int count) {
        if (count > 0) {
            print_line(name + ": {");
        }
    }

    std::string get_node_content() {
        std::string result = output.str();
        output.str("");
        output.clear();
        return result;
    }

public:
    // --- Base Node Types ---
    
    void visit(AstNode* node) override {
        print_line(std::string(g_ordered_type_infos[node->typeId]->name));
    }

    void visit(TokenNode* node) override {
        print_line("Token");
    }

    void visit(IdentifierNode* node) override {
        print_line("Identifier");
    }

    void visit(ErrorNode* node) override {
        print_line("Error");
    }

    // --- Expression Base ---
    
    void visit(ExpressionNode* node) override {
        print_line("Expression");
    }

    // --- Expression Implementations ---

    void visit(LiteralExpressionNode* node) override {
        print_line("LiteralExpression");
    }

    void visit(IdentifierExpressionNode* node) override {
        print_line("IdentifierExpression");
    }

    void visit(ParenthesizedExpressionNode* node) override {
        print_node_open("ParenthesizedExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->expression) {
            node->expression->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(UnaryExpressionNode* node) override {
        print_node_open("UnaryExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->operand) {
            node->operand->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(BinaryExpressionNode* node) override {
        print_node_open("BinaryExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->left) {
            node->left->accept(this);
        }
        if (node->right) {
            node->right->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(AssignmentExpressionNode* node) override {
        print_node_open("AssignmentExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->target) {
            node->target->accept(this);
        }
        if (node->source) {
            node->source->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(CallExpressionNode* node) override {
        print_node_open("CallExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->target) {
            node->target->accept(this);
        }
        if (node->arguments.size > 0) {
            print_collection("arguments", node->arguments.size);
            indentLevel++;
            for (int i = 0; i < node->arguments.size; i++) {
                if (node->arguments[i]) {
                    node->arguments[i]->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    void visit(MemberAccessExpressionNode* node) override {
        print_node_open("MemberAccessExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->target) {
            node->target->accept(this);
        }
        if (node->member) {
            print_line("member");
        }
        indentLevel--;
        print_node_close();
    }

    void visit(NewExpressionNode* node) override {
        print_node_open("NewExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->type) {
            node->type->accept(this);
        }
        if (node->constructorCall && node->constructorCall->arguments.size > 0) {
            print_collection("arguments", node->constructorCall->arguments.size);
            indentLevel++;
            for (int i = 0; i < node->constructorCall->arguments.size; i++) {
                if (node->constructorCall->arguments[i]) {
                    node->constructorCall->arguments[i]->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ThisExpressionNode* node) override {
        print_line("ThisExpression");
    }

    void visit(CastExpressionNode* node) override {
        print_node_open("CastExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->targetType) {
            node->targetType->accept(this);
        }
        if (node->expression) {
            node->expression->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(IndexerExpressionNode* node) override {
        print_node_open("IndexerExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->target) {
            node->target->accept(this);
        }
        if (node->index) {
            node->index->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(TypeOfExpressionNode* node) override {
        print_node_open("TypeOfExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->type) {
            node->type->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(SizeOfExpressionNode* node) override {
        print_node_open("SizeOfExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->type) {
            node->type->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(MatchExpressionNode* node) override {
        print_node_open("MatchExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->expression) {
            node->expression->accept(this);
        }
        if (node->arms.size > 0) {
            print_collection("arms", node->arms.size);
            indentLevel++;
            for (auto arm : node->arms) {
                if (arm) {
                    arm->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ConditionalExpressionNode* node) override {
        print_node_open("ConditionalExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->condition) {
            node->condition->accept(this);
        }
        if (node->whenTrue) {
            node->whenTrue->accept(this);
        }
        if (node->whenFalse) {
            node->whenFalse->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(RangeExpressionNode* node) override {
        print_node_open("RangeExpression");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->start) {
            node->start->accept(this);
        }
        if (node->end) {
            node->end->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(FieldKeywordExpressionNode* node) override {
        print_line("FieldKeywordExpression");
    }

    void visit(ValueKeywordExpressionNode* node) override {
        print_line("ValueKeywordExpression");
    }

    // --- Statement Base ---
    
    void visit(StatementNode* node) override {
        print_line("Statement");
    }

    // --- Statement Implementations ---

    void visit(EmptyStatementNode* node) override {
        print_line("EmptyStatement");
    }

    void visit(BlockStatementNode* node) override {
        print_node_open("BlockStatement");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        for (auto stmt : node->statements) {
            if (stmt) {
                stmt->accept(this);
            }
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ExpressionStatementNode* node) override {
        print_node_open("ExpressionStatement");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->expression) {
            node->expression->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(IfStatementNode* node) override {
        print_node_open("IfStatement");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->condition) {
            node->condition->accept(this);
        }
        if (node->thenStatement) {
            node->thenStatement->accept(this);
        }
        if (node->elseStatement) {
            node->elseStatement->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(WhileStatementNode* node) override {
        print_node_open("WhileStatement");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->condition) {
            node->condition->accept(this);
        }
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ForStatementNode* node) override {
        print_node_open("ForStatement");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->initializer) {
            node->initializer->accept(this);
        }
        if (node->condition) {
            node->condition->accept(this);
        }
        if (node->incrementors.size > 0) {
            print_collection("incrementors", node->incrementors.size);
            indentLevel++;
            for (int i = 0; i < node->incrementors.size; i++) {
                if (node->incrementors[i]) {
                    node->incrementors[i]->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ForInStatementNode* node) override {
        print_node_open("ForInStatement");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->mainVariable) {
            node->mainVariable->accept(this);
        }
        if (node->iterable) {
            node->iterable->accept(this);
        }
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ReturnStatementNode* node) override {
        print_node_open("ReturnStatement");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->expression) {
            node->expression->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(BreakStatementNode* node) override {
        print_line("BreakStatement");
    }

    void visit(ContinueStatementNode* node) override {
        print_line("ContinueStatement");
    }

    void visit(VariableDeclarationNode* node) override {
        print_node_open("VariableDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->type) {
            node->type->accept(this);
        }
        if (node->names.size > 0) {
            print_line("names");
        } else if (node->first_name()) {
            print_line("name");
        }
        if (node->initializer) {
            node->initializer->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    // --- Declaration Base ---
    
    void visit(DeclarationNode* node) override {
        print_node_open("Declaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        indentLevel--;
        print_node_close();
    }

    // --- Declaration Implementations ---

    void visit(NamespaceDeclarationNode* node) override {
        print_node_open("NamespaceDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(UsingDirectiveNode* node) override {
        print_node_open("UsingDirective");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->namespaceName) {
            node->namespaceName->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(TypeDeclarationNode* node) override {
        print_node_open("TypeDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        if (node->members.size > 0) {
            print_collection("members", node->members.size);
            indentLevel++;
            for (auto member : node->members) {
                if (member) {
                    member->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    void visit(InterfaceDeclarationNode* node) override {
        print_node_open("InterfaceDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        if (node->members.size > 0) {
            print_collection("members", node->members.size);
            indentLevel++;
            for (auto member : node->members) {
                if (member) {
                    member->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    void visit(EnumDeclarationNode* node) override {
        print_node_open("EnumDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        if (node->cases.size > 0) {
            print_collection("cases", node->cases.size);
            indentLevel++;
            for (auto enumCase : node->cases) {
                if (enumCase) {
                    enumCase->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        if (node->methods.size > 0) {
            print_collection("methods", node->methods.size);
            indentLevel++;
            for (auto method : node->methods) {
                if (method) {
                    method->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    void visit(MemberDeclarationNode* node) override {
        print_node_open("MemberDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        indentLevel--;
        print_node_close();
    }

    void visit(FunctionDeclarationNode* node) override {
        print_node_open("FunctionDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        if (node->parameters.size > 0) {
            print_collection("parameters", node->parameters.size);
            indentLevel++;
            for (int i = 0; i < node->parameters.size; i++) {
                if (node->parameters[i]) {
                    node->parameters[i]->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        if (node->returnType) {
            node->returnType->accept(this);
        }
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ParameterNode* node) override {
        print_node_open("Parameter");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->type) {
            node->type->accept(this);
        }
        if (node->name) {
            print_line("name");
        }
        if (node->defaultValue) {
            node->defaultValue->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(GenericParameterNode* node) override {
        print_node_open("GenericParameter");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        indentLevel--;
        print_node_close();
    }

    void visit(PropertyDeclarationNode* node) override {
        print_node_open("PropertyDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        if (node->type) {
            node->type->accept(this);
        }
        if (node->getterExpression) {
            node->getterExpression->accept(this);
        }
        if (node->accessors.size > 0) {
            print_collection("accessors", node->accessors.size);
            indentLevel++;
            for (auto accessor : node->accessors) {
                if (accessor) {
                    accessor->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    void visit(PropertyAccessorNode* node) override {
        print_node_open("PropertyAccessor");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->accessorKeyword) {
            print_line("accessorKeyword");
        }
        if (node->expression) {
            node->expression->accept(this);
        }
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ConstructorDeclarationNode* node) override {
        print_node_open("ConstructorDeclaration");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->parameters.size > 0) {
            print_collection("parameters", node->parameters.size);
            indentLevel++;
            for (int i = 0; i < node->parameters.size; i++) {
                if (node->parameters[i]) {
                    node->parameters[i]->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(EnumCaseNode* node) override {
        print_node_open("EnumCase");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->modifiers.size > 0) {
            print_line("modifiers");
        }
        if (node->name) {
            print_line("name");
        }
        if (node->associatedData.size > 0) {
            print_collection("associatedData", node->associatedData.size);
            indentLevel++;
            for (int i = 0; i < node->associatedData.size; i++) {
                if (node->associatedData[i]) {
                    node->associatedData[i]->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    // --- Match Pattern Implementations ---

    void visit(MatchArmNode* node) override {
        print_node_open("MatchArm");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->pattern) {
            node->pattern->accept(this);
        }
        if (node->result) {
            node->result->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(MatchPatternNode* node) override {
        print_line("MatchPattern");
    }

    void visit(EnumPatternNode* node) override {
        print_node_open("EnumPattern");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->enumCase) {
            print_line("enumCase");
        }
        indentLevel--;
        print_node_close();
    }

    void visit(RangePatternNode* node) override {
        print_node_open("RangePattern");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->start) {
            node->start->accept(this);
        }
        if (node->end) {
            node->end->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(ComparisonPatternNode* node) override {
        print_node_open("ComparisonPattern");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->comparisonOp) {
            print_line("comparisonOp");
        }
        if (node->value) {
            node->value->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(WildcardPatternNode* node) override {
        print_line("WildcardPattern");
    }

    void visit(LiteralPatternNode* node) override {
        print_node_open("LiteralPattern");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->literal) {
            node->literal->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    // --- Type Name Implementations ---

    void visit(QualifiedNameNode* node) override {
        print_inline("QualifiedName");
    }

    void visit(TypeNameNode* node) override {
        print_line("TypeName");
    }

    void visit(ArrayTypeNameNode* node) override {
        print_node_open("ArrayTypeName");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->elementType) {
            node->elementType->accept(this);
        }
        indentLevel--;
        print_node_close();
    }

    void visit(GenericTypeNameNode* node) override {
        print_node_open("GenericTypeName");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->baseType) {
            node->baseType->accept(this);
        }
        if (node->arguments.size > 0) {
            print_collection("arguments", node->arguments.size);
            indentLevel++;
            for (int i = 0; i < node->arguments.size; i++) {
                if (node->arguments[i]) {
                    node->arguments[i]->accept(this);
                }
            }
            indentLevel--;
            print_node_close();
        }
        indentLevel--;
        print_node_close();
    }

    // --- Root ---
    
    void visit(CompilationUnitNode* node) override {
        print_node_open("CompilationUnit");
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        for (auto stmt : node->statements) {
            if (stmt) {
                stmt->accept(this);
            }
        }
        indentLevel--;
        print_node_close();
    }
};