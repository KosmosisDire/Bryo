#pragma once

#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_rtti.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <sstream>

using namespace Myre;

// A code-like visitor to print AST nodes as pseudo-code
class AstToCodePrinter : public StructuralVisitor
{
private:
    int indentLevel = 0;
    std::ostringstream output;
    bool suppressNextNewline = false;
    bool suppressSemicolon = false;

    std::string get_indent() {
        std::string indent;
        for (int i = 0; i < indentLevel; ++i) indent += "  ";
        return indent;
    }

    void emit(const std::string& text) {
        output << text;
    }

    void emit_line(const std::string& text) {
        output << get_indent() << text << "\n";
    }

    void emit_indent() {
        output << get_indent();
    }

    void emit_newline() {
        if (!suppressNextNewline) {
            output << "\n";
        }
        suppressNextNewline = false;
    }

    void print_modifiers(const SizedArray<ModifierKind>& modifiers) {
        for (int i = 0; i < modifiers.size; i++) {
            emit(std::string(to_string(modifiers[i])));
            emit(" ");
        }
    }

public:
    std::string get_result() {
        std::string result = output.str();
        // Log the final result
        std::istringstream stream(result);
        std::string line;
        while (std::getline(stream, line)) {
            LOG_INFO(line, LogCategory::AST);
        }
        return result;
    }

    // --- Base Node Types ---

    void visit(TokenNode* node) override {
        emit("\"" + std::string(node->text) + "\"");
    }

    void visit(IdentifierNode* node) override {
        emit(std::string(node->name));
    }

    void visit(ErrorNode* node) override {
        emit_line("[ERROR]");
    }

    // --- Expression Base ---
    
    void visit(ExpressionNode* node) override {
        emit("[AbstractExpression]");
    }

    // --- Expression Implementations ---

    void visit(LiteralExpressionNode* node) override {
        emit(std::string(node->token->text));
    }

    void visit(IdentifierExpressionNode* node) override {
        emit(std::string(node->identifier->name));
    }

    void visit(ParenthesizedExpressionNode* node) override {
        emit("(");
        if (node->expression) {
            node->expression->accept(this);
        }
        emit(")");
    }

    void visit(UnaryExpressionNode* node) override {
        if (node->isPostfix) {
            if (node->operand) {
                node->operand->accept(this);
            }
            emit(std::string(to_string(node->opKind)));
        } else {
            emit(std::string(to_string(node->opKind)));
            if (node->operand) {
                node->operand->accept(this);
            }
        }
    }

    void visit(BinaryExpressionNode* node) override {
        if (node->left) {
            node->left->accept(this);
        } else {
            emit("[missing-left]");
        }
        emit(" " + std::string(to_string(node->opKind)) + " ");
        if (node->right) {
            node->right->accept(this);
        } else {
            emit("[missing-right]");
        }
    }

    void visit(AssignmentExpressionNode* node) override {
        if (node->target) {
            node->target->accept(this);
        }
        emit(" " + std::string(to_string(node->opKind)) + " ");
        if (node->source) {
            node->source->accept(this);
        }
    }

    void visit(CallExpressionNode* node) override {
        if (node->target) {
            node->target->accept(this);
        }
        emit("(");
        for (int i = 0; i < node->arguments.size; i++) {
            if (i > 0) emit(", ");
            if (node->arguments[i]) {
                node->arguments[i]->accept(this);
            } else {
                emit("[missing-arg]");
            }
        }
        emit(")");
    }

    void visit(MemberAccessExpressionNode* node) override {
        if (node->target) {
            node->target->accept(this);
        }
        emit(".");
        if (node->member) {
            emit(std::string(node->member->name));
        }
    }

    void visit(NewExpressionNode* node) override {
        emit("new ");
        if (node->type) {
            node->type->accept(this);
        }
        if (node->constructorCall) {
            // Print just the arguments part since type is already printed
            emit("(");
            for (int i = 0; i < node->constructorCall->arguments.size; i++) {
                if (i > 0) emit(", ");
                if (node->constructorCall->arguments[i]) {
                    node->constructorCall->arguments[i]->accept(this);
                }
            }
            emit(")");
        }
    }

    void visit(ThisExpressionNode* node) override {
        emit("this");
    }

    void visit(CastExpressionNode* node) override {
        emit("(");
        if (node->targetType) {
            node->targetType->accept(this);
        }
        emit(")");
        if (node->expression) {
            node->expression->accept(this);
        }
    }

    void visit(IndexerExpressionNode* node) override {
        if (node->target) {
            node->target->accept(this);
        }
        emit("[");
        if (node->index) {
            node->index->accept(this);
        }
        emit("]");
    }

    void visit(TypeOfExpressionNode* node) override {
        emit("typeof(");
        if (node->type) {
            node->type->accept(this);
        }
        emit(")");
    }

    void visit(SizeOfExpressionNode* node) override {
        emit("sizeof(");
        if (node->type) {
            node->type->accept(this);
        }
        emit(")");
    }

    void visit(MatchExpressionNode* node) override {
        emit("match (");
        if (node->expression) {
            node->expression->accept(this);
        }
        emit(") {");
        
        if (!node->arms.empty()) {
            emit_newline();
            indentLevel++;
            for (auto arm : node->arms) {
                if (arm) {
                    arm->accept(this);
                }
            }
            indentLevel--;
            emit_indent();
            emit("}");
        } else {
            emit(" }");
        }
    }

    void visit(ConditionalExpressionNode* node) override {
        if (node->condition) {
            node->condition->accept(this);
        }
        emit(" ? ");
        if (node->whenTrue) {
            node->whenTrue->accept(this);
        }
        emit(" : ");
        if (node->whenFalse) {
            node->whenFalse->accept(this);
        }
    }

    void visit(RangeExpressionNode* node) override {
        if (node->start) {
            node->start->accept(this);
        }
        if (node->rangeOp) {
            emit(std::string(node->rangeOp->text));
        } else {
            emit("..");
        }
        if (node->end) {
            node->end->accept(this);
        }
        // Handle optional "by" clause for step
        if (node->byKeyword && node->stepExpression) {
            emit(" by ");
            node->stepExpression->accept(this);
        }
    }

    void visit(FieldKeywordExpressionNode* node) override {
        emit("field");
    }

    void visit(ValueKeywordExpressionNode* node) override {
        emit("value");
    }

    // --- Statement Base ---
    
    void visit(StatementNode* node) override {
        emit_line("[AbstractStatement]");
    }

    // --- Statement Implementations ---

    void visit(EmptyStatementNode* node) override {
        emit_line(";");
    }

    void visit(BlockStatementNode* node) override {
        emit_line("{");
        indentLevel++;
        for (auto stmt : node->statements) {
            if (stmt) {
                stmt->accept(this);
            }
        }
        indentLevel--;
        emit_line("}");
    }

    void visit(ExpressionStatementNode* node) override {
        emit_indent();
        if (node->expression) {
            node->expression->accept(this);
        }
        emit(";");
        emit_newline();
    }

    void visit(IfStatementNode* node) override {
        emit_indent();
        emit("if (");
        if (node->condition) {
            node->condition->accept(this);
        }
        emit(")");
        emit_newline();
        
        if (node->thenStatement) {
            node->thenStatement->accept(this);
        }
        
        if (node->elseStatement) {
            emit_line("else");
            node->elseStatement->accept(this);
        }
    }

    void visit(WhileStatementNode* node) override {
        emit_indent();
        emit("while (");
        if (node->condition) {
            node->condition->accept(this);
        }
        emit(")");
        emit_newline();
        
        if (node->body) {
            node->body->accept(this);
        }
    }

    void visit(ForStatementNode* node) override {
        emit_indent();
        emit("for (");
        
        // Initializer
        if (node->initializer) {
            suppressNextNewline = true;
            // Don't suppress semicolon here since regular for loops need it
            node->initializer->accept(this);
            suppressNextNewline = false;
            emit(" ");
        }
        else
        {
            emit("; ");
        }
        
        // Condition
        if (node->condition) {
            node->condition->accept(this);
        }
        emit("; ");
        
        // Incrementors
        for (int i = 0; i < node->incrementors.size; i++) {
            if (i > 0) emit(", ");
            if (node->incrementors[i]) {
                node->incrementors[i]->accept(this);
            }
        }
        emit(")");
        emit_newline();
        
        if (node->body) {
            node->body->accept(this);
        }
    }

    void visit(ForInStatementNode* node) override {
        emit_indent();
        emit("for (");
        if (node->mainVariable) {
            suppressNextNewline = true;
            suppressSemicolon = true;
            node->mainVariable->accept(this);
            suppressNextNewline = false;
            suppressSemicolon = false;
        }
        emit(" in ");
        if (node->iterable) {
            node->iterable->accept(this);
        }
        // Handle optional "at" clause for index variable
        if (node->atKeyword && node->indexVariable) {
            emit(" at ");
            suppressNextNewline = true;
            suppressSemicolon = true;
            node->indexVariable->accept(this);
            suppressNextNewline = false;
            suppressSemicolon = false;
        }
        emit(")");
        emit_newline();
        
        if (node->body) {
            node->body->accept(this);
        }
    }

    void visit(ReturnStatementNode* node) override {
        emit_indent();
        emit("return");
        if (node->expression) {
            emit(" ");
            node->expression->accept(this);
        }
        emit(";");
        emit_newline();
    }

    void visit(BreakStatementNode* node) override {
        emit_line("break;");
    }

    void visit(ContinueStatementNode* node) override {
        emit_line("continue;");
    }

    void visit(VariableDeclarationNode* node) override {
        if (!suppressNextNewline) {
            emit_indent();
        }
        
        print_modifiers(node->modifiers);
        
        if (node->varKeyword) {
            emit("var ");
        }
        else if (node->type) {
            node->type->accept(this);
            emit(" ");
        }
        else
        {
            emit("[Print Error: Missing var/type] ");
        }
        
        // Print names
        if (node->names.size > 0) {
            for (int i = 0; i < node->names.size; i++) {
                if (i > 0) emit(", ");
                if (node->names[i]) {
                    emit(std::string(node->names[i]->name));
                }
            }
        }
        
        // Initializer
        if (node->initializer) {
            emit(" = ");
            node->initializer->accept(this);
        } 
        
        if (!suppressSemicolon) {
            emit(";");
        }
        
        if (!suppressNextNewline) {
            emit_newline();
        }
        
        // Reset flags
        suppressSemicolon = false;
        suppressNextNewline = false;
    }

    // --- Declaration Base ---
    
    void visit(DeclarationNode* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit("[AbstractDeclaration]");
        emit_newline();
    }

    // --- Declaration Implementations ---

    void visit(NamespaceDeclarationNode* node) override {
        emit_newline();
        emit_indent();
        print_modifiers(node->modifiers);
        emit("namespace ");
        if (node->name) {
            emit(node->name->get_full_name());
        }
        emit_newline();
        
        if (node->body) {
            node->body->accept(this);
        }
    }

    void visit(UsingDirectiveNode* node) override {
        emit_indent();
        emit("using ");
        if (node->namespaceName) {
            node->namespaceName->accept(this);
        }
        emit(";");
        emit_newline();
    }

    void visit(TypeDeclarationNode* node) override {
        emit_newline();
        emit_indent();
        print_modifiers(node->modifiers);
        emit("type ");
        if (node->name) {
            emit(std::string(node->name->name));
        }
        emit(" {");
        emit_newline();
        
        indentLevel++;
        for (auto member : node->members) {
            if (member) {
                member->accept(this);
            }
        }
        indentLevel--;
        emit_line("}");
    }

    void visit(InterfaceDeclarationNode* node) override {
        emit_newline();
        emit_indent();
        print_modifiers(node->modifiers);
        emit("interface ");
        if (node->name) {
            emit(std::string(node->name->name));
        }
        emit(" {");
        emit_newline();
        
        indentLevel++;
        for (auto member : node->members) {
            if (member) {
                member->accept(this);
            }
        }
        indentLevel--;
        emit_line("}");
    }

    void visit(EnumDeclarationNode* node) override {
        emit_newline();
        emit_indent();
        print_modifiers(node->modifiers);
        emit("enum ");
        if (node->name) {
            emit(std::string(node->name->name));
        }
        emit(" {");
        emit_newline();
        
        indentLevel++;
        
        // Print cases
        for (auto enumCase : node->cases) {
            if (enumCase) {
                enumCase->accept(this);
            }
        }
        
        // Print methods
        for (auto method : node->methods) {
            if (method) {
                method->accept(this);
            }
        }
        
        indentLevel--;
        emit_line("}");
    }

    void visit(MemberDeclarationNode* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        std::string name = node->name ? std::string(node->name->name) : "[unnamed]";
        emit("[AbstractMember: " + name + "]");
        emit_newline();
    }

    void visit(FunctionDeclarationNode* node) override {
        emit_newline();
        emit_indent();
        print_modifiers(node->modifiers);
        emit("fn ");
        if (node->name) {
            emit(std::string(node->name->name));
        }
        
        emit("(");
        for (int i = 0; i < node->parameters.size; i++) {
            if (i > 0) emit(", ");
            if (node->parameters[i]) {
                node->parameters[i]->accept(this);
            }
        }
        emit(")");
        
        // Return type
        if (node->returnType) {
            emit(": ");
            node->returnType->accept(this);
        }
        
        if (node->body) {
            emit_newline();
            node->body->accept(this);
        } else {
            emit(";");
            emit_newline();
        }
    }

    void visit(ParameterNode* node) override {
        print_modifiers(node->modifiers);
        if (node->type) {
            node->type->accept(this);
            emit(" ");
        }
        if (node->name) {
            emit(std::string(node->name->name));
        }
        if (node->defaultValue) {
            emit(" = ");
            node->defaultValue->accept(this);
        }
    }

    void visit(GenericParameterNode* node) override {
        print_modifiers(node->modifiers);
        if (node->name) {
            emit(std::string(node->name->name));
        }
    }

    void visit(PropertyDeclarationNode* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        if (node->name) {
            emit(std::string(node->name->name));
        }
        if (node->type) {
            emit(": ");
            node->type->accept(this);
        }

        if (node->equals) {
            emit(" = ");
            node->initializer->accept(this);
        }

        if (node->getterExpression) {
            emit(" => ");
            node->getterExpression->accept(this);
            emit(";");
            emit_newline();
        } else if (!node->accessors.empty()) {
            emit(" {");
            emit_newline();
            
            indentLevel++;
            for (auto accessor : node->accessors) {
                if (accessor) {
                    accessor->accept(this);
                }
            }
            indentLevel--;
            emit_line("}");
        } else {
            emit(";");
            emit_newline();
        }
    }

    void visit(PropertyAccessorNode* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        if (node->accessorKeyword) {
            emit(std::string(node->accessorKeyword->text));
        }
        
        if (node->expression) {
            emit(" => ");
            node->expression->accept(this);
            emit(";");
            emit_newline();
        } else if (node->body) {
            emit_newline();
            node->body->accept(this);
        } else {
            emit(";");
            emit_newline();
        }
    }

    void visit(ConstructorDeclarationNode* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit("new(");
        for (int i = 0; i < node->parameters.size; i++) {
            if (i > 0) emit(", ");
            if (node->parameters[i]) {
                node->parameters[i]->accept(this);
            }
        }
        emit(")");
        emit_newline();
        
        if (node->body) {
            node->body->accept(this);
        }
    }

    void visit(EnumCaseNode* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit("case ");
        if (node->name) {
            emit(std::string(node->name->name));
        }
        
        if (!node->associatedData.empty()) {
            emit("(");
            for (int i = 0; i < node->associatedData.size; i++) {
                if (i > 0) emit(", ");
                if (node->associatedData[i]) {
                    node->associatedData[i]->accept(this);
                }
            }
            emit(")");
        }
        
        emit(";");
        emit_newline();
    }

    // --- Match Pattern Implementations ---

    void visit(MatchArmNode* node) override {
        emit_indent();
        if (node->pattern) {
            node->pattern->accept(this);
        }
        emit(" => ");
        if (node->result) {
            node->result->accept(this);
        }
        emit(",");
        emit_newline();
    }

    void visit(MatchPatternNode* node) override {
        emit("[AbstractPattern]");
    }

    void visit(EnumPatternNode* node) override {
        emit(".");
        if (node->enumCase) {
            emit(std::string(node->enumCase->name));
        }
    }

    void visit(RangePatternNode* node) override {
        if (node->start) {
            node->start->accept(this);
        }
        if (node->rangeOp) {
            emit(std::string(node->rangeOp->text));
        } else {
            emit("..");
        }
        if (node->end) {
            node->end->accept(this);
        }
    }

    void visit(ComparisonPatternNode* node) override {
        if (node->comparisonOp) {
            emit(std::string(node->comparisonOp->text));
        }
        emit(" ");
        if (node->value) {
            node->value->accept(this);
        }
    }

    void visit(WildcardPatternNode* node) override {
        emit("_");
    }

    void visit(LiteralPatternNode* node) override {
        if (node->literal) {
            node->literal->accept(this);
        }
    }

    // --- Type Name Implementations ---

    void visit(QualifiedNameNode* node) override {
        emit(node->get_full_name());
    }

    void visit(TypeNameNode* node) override {
        if (node->name->identifiers.size > 0) {
            emit(node->get_full_name());
        }
    }

    void visit(ArrayTypeNameNode* node) override {
        if (node->elementType) {
            node->elementType->accept(this);
        }
        emit("[]");
    }

    void visit(GenericTypeNameNode* node) override {
        if (node->baseType) {
            node->baseType->accept(this);
        }
        emit("<");
        for (int i = 0; i < node->arguments.size; i++) {
            if (i > 0) emit(", ");
            if (node->arguments[i]) {
                node->arguments[i]->accept(this);
            }
        }
        emit(">");
    }

    // --- Root ---
    
    void visit(CompilationUnitNode* node) override {
        for (auto stmt : node->statements) {
            if (stmt) {
                stmt->accept(this);
            }
        }
    }
};