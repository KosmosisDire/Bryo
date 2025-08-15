#pragma once

#include "ast/ast.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <sstream>
#include <variant>

namespace Myre {

class AstToCodePrinter : public Visitor {
private:
    int indentLevel = 0;
    std::ostringstream output;

    // RAII helper for managing indentation levels safely.
    class IndentGuard {
    private:
        int& level;
    public:
        IndentGuard(int& level) : level(level) { this->level++; }
        ~IndentGuard() { this->level--; }
    };

    std::string get_indent() {
        return std::string(indentLevel * 2, ' ');
    }

    void emit(const std::string& text) {
        output << text;
    }

    void emit_indent() {
        output << get_indent();
    }

    void emit_newline() {
        output << "\n";
    }

    void print_modifiers(const ModifierKindFlags& modifiers) {
        emit(to_string(modifiers) + " ");
    }
    
    void print_body(Statement* body) {
        if (!body) {
            emit(";\n");
            return;
        }
        
        if (body->is<Block>()) {
            emit(" ");
            body->accept(this);
            emit_newline();
        } else {
            emit_newline();
            IndentGuard guard(indentLevel);
            body->accept(this);
        }
    }

public:
    std::string get_result() {
        std::string result = output.str();
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        return result;
    }

    // --- Base Node Types (unchanged) ---
    void visit(Node* node) override { emit("[AbstractNode]"); }
    void visit(Expression* node) override { emit("[AbstractExpression]"); }
    void visit(Statement* node) override { emit_indent(); emit("[AbstractStatement]"); emit_newline(); }
    void visit(Declaration* node) override { emit_indent(); print_modifiers(node->modifiers); emit("[AbstractDeclaration]"); emit_newline(); }
    void visit(Pattern* node) override { emit("[AbstractPattern]"); }
    void visit(TypeRef* node) override { emit("[AbstractTypeRef]"); }
    void visit(TypeConstraint* node) override { emit("[AbstractConstraint]"); }

    // --- Basic Building Blocks & Errors (unchanged) ---
    void visit(Identifier* node) override { emit(std::string(node->text)); }
    void visit(TypedIdentifier* node) override {
        if (node->type) {
            node->type->accept(this);
            emit(" ");
        } else {
            emit("var ");
        }
        if (node->name) node->name->accept(this);
    }
    void visit(ErrorExpression* node) override { emit("[ERROR: " + std::string(node->message) + "]"); }
    void visit(ErrorStatement* node) override { emit_indent(); emit("[ERROR: " + std::string(node->message) + "]"); emit_newline(); }
    void visit(ErrorTypeRef* node) override { emit("[ERROR: " + std::string(node->message) + "]"); }
    
    // --- Expressions (unchanged) ---
    void visit(LiteralExpr* node) override { emit(std::string(node->value)); }
    void visit(ArrayLiteralExpr* node) override {
        emit("[");
        for (size_t i = 0; i < node->elements.size(); i++) {
            if (i > 0) emit(", ");
            if (node->elements[i]) node->elements[i]->accept(this);
        }
        emit("]");
    }
    void visit(NameExpr* node) override {
        for (size_t i = 0; i < node->parts.size(); i++) {
            if (i > 0) emit(".");
            if (node->parts[i]) node->parts[i]->accept(this);
        }
    }
    void visit(UnaryExpr* node) override {
        if (node->isPostfix) {
            node->operand->accept(this);
            emit(std::string(to_string(node->op)));
        } else {
            emit(std::string(to_string(node->op)));
            node->operand->accept(this);
        }
    }
    void visit(BinaryExpr* node) override {
        node->left->accept(this);
        emit(" " + std::string(to_string(node->op)) + " ");
        node->right->accept(this);
    }
    void visit(AssignmentExpr* node) override {
        node->target->accept(this);
        emit(" " + std::string(to_string(node->op)) + " ");
        node->value->accept(this);
    }
    void visit(CallExpr* node) override {
        node->callee->accept(this);
        emit("(");
        for (size_t i = 0; i < node->arguments.size(); i++) {
            if (i > 0) emit(", ");
            if (node->arguments[i]) node->arguments[i]->accept(this);
        }
        emit(")");
    }
    void visit(MemberAccessExpr* node) override {
        node->object->accept(this);
        emit(".");
        node->member->accept(this);
    }
    void visit(IndexerExpr* node) override {
        node->object->accept(this);
        emit("[");
        node->index->accept(this);
        emit("]");
    }
    void visit(CastExpr* node) override {
        emit("(");
        node->targetType->accept(this);
        emit(")");
        node->expression->accept(this);
    }
    void visit(NewExpr* node) override {
        emit("new ");
        node->type->accept(this);
        emit("(");
        for (size_t i = 0; i < node->arguments.size(); i++) {
            if (i > 0) emit(", ");
            if (node->arguments[i]) node->arguments[i]->accept(this);
        }
        emit(")");
    }
    void visit(ThisExpr* node) override { emit("this"); }
    void visit(LambdaExpr* node) override {
        emit("(");
        for (size_t i = 0; i < node->parameters.size(); i++) {
            if (i > 0) emit(", ");
            if (node->parameters[i]) node->parameters[i]->accept(this);
        }
        emit(") => ");
        if (node->body) node->body->accept(this);
    }
    void visit(RangeExpr* node) override {
        if (node->start) node->start->accept(this);
        emit(node->isInclusive ? "..=" : "..");
        if (node->end) node->end->accept(this);
        if (node->step) {
            emit(" by ");
            node->step->accept(this);
        }
    }
    void visit(ConditionalExpr* node) override {
        node->condition->accept(this);
        emit(" ? ");
        node->thenExpr->accept(this);
        emit(" : ");
        node->elseExpr->accept(this);
    }
    void visit(TypeOfExpr* node) override { emit("typeof("); node->type->accept(this); emit(")"); }
    void visit(SizeOfExpr* node) override { emit("sizeof("); node->type->accept(this); emit(")"); }

    // --- CORRECTED VISITORS ---

    void visit(Block* node) override {
        emit("{");
        emit_newline();
        
        // The guard's scope is explicitly limited to the loop.
        {
            IndentGuard guard(indentLevel);
            for (auto stmt : node->statements) {
                if (stmt) stmt->accept(this);
            }
        } // Guard is destroyed here, indent level is restored.
        
        emit_indent(); // This now uses the correct, outer indent level.
        emit("}");     // The caller adds the final newline if needed.
    }

    void visit(IfExpr* node) override {
        emit("if (");
        node->condition->accept(this);
        emit(")");

        print_body(node->thenBranch);
        
        if (node->elseBranch) {
            output.seekp(-1, std::ios_base::end); 
            emit(" else");

            if (node->elseBranch->is<IfExpr>()) {
                emit(" ");
                node->elseBranch->accept(this);
            } else {
                print_body(node->elseBranch);
            }
        }
    }

    void visit(MatchExpr* node) override {
        emit("match (");
        node->subject->accept(this);
        emit(") {");
        emit_newline();
        
        {
            IndentGuard guard(indentLevel);
            for (auto arm : node->arms) {
                if (arm) arm->accept(this);
            }
        }
        
        emit_indent();
        emit("}");
    }
    
    void visit(MatchArm* node) override {
        emit_indent();
        node->pattern->accept(this);
        emit(" => ");
        
        if (node->result && node->result->is<Block>()) {
            node->result->accept(this);
            emit_newline();
        } else if(node->result) {
            node->result->accept(this);
            emit(",");
            emit_newline();
        } else {
            emit ("{},"); // Empty arm
            emit_newline();
        }
    }

    // --- Statements (unchanged) ---
    void visit(ExpressionStmt* node) override {
        emit_indent();
        node->expression->accept(this);
        if (!node->expression->is<IfExpr>() && !node->expression->is<MatchExpr>() &&
            !node->expression->is<Block>()) {
            emit(";");
        }
        emit_newline();
    }
    void visit(ReturnStmt* node) override {
        emit_indent();
        emit("return");
        if (node->value) {
            emit(" ");
            node->value->accept(this);
        }
        emit(";");
        emit_newline();
    }
    void visit(BreakStmt* node) override { emit_indent(); emit("break;"); emit_newline(); }
    void visit(ContinueStmt* node) override { emit_indent(); emit("continue;"); emit_newline(); }
    void visit(WhileStmt* node) override {
        emit_indent();
        emit("while (");
        node->condition->accept(this);
        emit(")");
        print_body(node->body);
    }
    void visit(ForStmt* node) override {
        emit_indent();
        emit("for (");
        if (node->initializer) {
            if (auto* varDecl = node->initializer->as<VariableDecl>()) {
                print_modifiers(varDecl->modifiers);
                varDecl->variable->accept(this);
                if (varDecl->initializer) {
                    emit(" = ");
                    varDecl->initializer->accept(this);
                }
            } else if (auto* exprStmt = node->initializer->as<ExpressionStmt>()) {
                exprStmt->expression->accept(this);
            }
        }
        emit("; ");
        if (node->condition) node->condition->accept(this);
        emit("; ");
        for (size_t i = 0; i < node->updates.size(); i++) {
            if (i > 0) emit(", ");
            if (node->updates[i]) node->updates[i]->accept(this);
        }
        emit(")");
        print_body(node->body);
    }
    void visit(ForInStmt* node) override {
        emit_indent();
        emit("for (");
        node->iterator->accept(this);
        emit(" in ");
        node->iterable->accept(this);
        if (node->indexVar) {
            emit(" at ");
            node->indexVar->accept(this);
        }
        emit(")");
        print_body(node->body);
    }
    void visit(UsingDirective* node) override {
        emit_indent();
        emit("using ");
        if (node->kind == UsingDirective::Kind::Alias && node->alias) {
            node->alias->accept(this);
            emit(" = ");
            if (node->aliasedType) node->aliasedType->accept(this);
        } else {
            for (size_t i = 0; i < node->path.size(); i++) {
                if (i > 0) emit(".");
                if (node->path[i]) node->path[i]->accept(this);
            }
        }
        emit(";");
        emit_newline();
    }

    // --- Declarations (with corrections) ---
    void visit(VariableDecl* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        node->variable->accept(this);
        if (node->initializer) {
            emit(" = ");
            node->initializer->accept(this);
        }
        emit(";");
        emit_newline();
    }

    void visit(MemberVariableDecl* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        
        if (node->type) {
            node->type->accept(this);
            emit(" ");
        }
        node->name->accept(this);
        
        if (node->initializer) {
            emit(" = ");
            node->initializer->accept(this);
        }
        
        if (node->getter || node->setter) {
            emit(" {");
            emit_newline();
            {
                IndentGuard guard(indentLevel);
                if (node->getter) node->getter->accept(this);
                if (node->setter) node->setter->accept(this);
            }
            emit_indent();
            emit("}");
            emit_newline();
        } else {
            emit(";");
            emit_newline();
        }
    }
    
    void visit(ParameterDecl* node) override {
        print_modifiers(node->modifiers);
        node->param->accept(this);
        if (node->defaultValue) {
            emit(" = ");
            node->defaultValue->accept(this);
        }
    }
    void visit(GenericParamDecl* node) override {
        print_modifiers(node->modifiers);
        node->name->accept(this);
    }
    void visit(FunctionDecl* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit("fn ");
        node->name->accept(this);
        
        if (!node->genericParams.empty()) {
            emit("<");
            for (size_t i = 0; i < node->genericParams.size(); i++) {
                if (i > 0) emit(", ");
                node->genericParams[i]->accept(this);
            }
            emit(">");
        }
        
        emit("(");
        for (size_t i = 0; i < node->parameters.size(); i++) {
            if (i > 0) emit(", ");
            node->parameters[i]->accept(this);
        }
        emit(")");
        
        if (node->returnType) {
            emit(": ");
            node->returnType->accept(this);
        }
        
        if (node->body) {
            emit_newline();
            emit_indent();
            node->body->accept(this);
            emit_newline();
        } else {
            emit(";");
            emit_newline();
        }
    }
    void visit(ConstructorDecl* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit("new(");
        for (size_t i = 0; i < node->parameters.size(); i++) {
            if (i > 0) emit(", ");
            node->parameters[i]->accept(this);
        }
        emit(")");
        emit_newline();
        emit_indent();
        node->body->accept(this);
        emit_newline();
    }
    void visit(PropertyAccessor* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit(node->kind == PropertyAccessor::Kind::Get ? "get" : "set");
        
        if (auto* expr = std::get_if<Expression*>(&node->body)) {
            emit(" => ");
            (*expr)->accept(this);
            emit(";");
        } else if (auto* block = std::get_if<Block*>(&node->body)) {
            emit(" ");
            (*block)->accept(this);
        } else {
            emit(";");
        }
        emit_newline();
    }
    void visit(InheritFunctionDecl* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit("inherit fn ");
        node->functionName->accept(this);
        if (!node->parameterTypes.empty()) {
            emit("(");
            for (size_t i = 0; i < node->parameterTypes.size(); i++) {
                if (i > 0) emit(", ");
                node->parameterTypes[i]->accept(this);
            }
            emit(")");
        }
        emit(";");
        emit_newline();
    }
    void visit(EnumCaseDecl* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit("case ");
        node->name->accept(this);
        if (!node->associatedData.empty()) {
            emit("(");
            for (size_t i = 0; i < node->associatedData.size(); i++) {
                if (i > 0) emit(", ");
                node->associatedData[i]->accept(this);
            }
            emit(")");
        }
        emit(",");
        emit_newline();
    }

    void visit(TypeDecl* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        switch (node->kind) {
            case TypeDecl::Kind::Type:       emit("type "); break;
            case TypeDecl::Kind::ValueType:  emit("value type "); break;
            case TypeDecl::Kind::RefType:    emit("ref type "); break;
            case TypeDecl::Kind::StaticType: emit("static type "); break;
            case TypeDecl::Kind::Enum:       emit("enum "); break;
        }
        node->name->accept(this);
        
        if (!node->genericParams.empty()) {
            emit("<");
            for (size_t i = 0; i < node->genericParams.size(); i++) {
                if (i > 0) emit(", ");
                node->genericParams[i]->name->accept(this);
            }
            emit(">");
        }
        
        if (!node->baseTypes.empty()) {
            emit(" : ");
            for (size_t i = 0; i < node->baseTypes.size(); i++) {
                if (i > 0) emit(", ");
                node->baseTypes[i]->accept(this);
            }
        }

        bool firstConstraint = true;
        for (auto* param : node->genericParams) {
            if (!param->constraints.empty()) {
                if (firstConstraint) {
                    emit(" where ");
                    firstConstraint = false;
                } else {
                    emit(", ");
                }
                param->name->accept(this);
                emit(" : ");
                for (size_t j = 0; j < param->constraints.size(); j++) {
                    if (j > 0) emit(", ");
                    param->constraints[j]->accept(this);
                }
            }
        }
        
        emit_newline();
        emit_indent();
        emit("{");
        emit_newline();
        
        {
            IndentGuard guard(indentLevel);
            for (auto* member : node->members) {
                if (member) member->accept(this);
            }
        }
        
        emit_indent();
        emit("}");
        emit_newline();
    }

    void visit(NamespaceDecl* node) override {
        emit_indent();
        print_modifiers(node->modifiers);
        emit("namespace ");
        for (size_t i = 0; i < node->path.size(); i++) {
            if (i > 0) emit(".");
            node->path[i]->accept(this);
        }
        
        if (node->isFileScoped) {
            emit(";");
            emit_newline();
        } else if (node->body) {
            emit_newline();
            emit_indent();
            emit("{");
            emit_newline();
            {
                IndentGuard guard(indentLevel);
                for (auto* stmt : *node->body) {
                    if (stmt) stmt->accept(this);
                }
            }
            emit_indent();
            emit("}");
            emit_newline();
        }
    }

    // --- Type References & Patterns (unchanged) ---
    void visit(NamedTypeRef* node) override {
        for (size_t i = 0; i < node->path.size(); i++) {
            if (i > 0) emit(".");
            node->path[i]->accept(this);
        }
        if (!node->genericArgs.empty()) {
            emit("<");
            for (size_t i = 0; i < node->genericArgs.size(); i++) {
                if (i > 0) emit(", ");
                node->genericArgs[i]->accept(this);
            }
            emit(">");
        }
    }
    void visit(ArrayTypeRef* node) override { node->elementType->accept(this); emit("[]"); }
    void visit(FunctionTypeRef* node) override {
        emit("Fn<");
        if (!node->parameterTypes.empty() || node->returnType) {
            emit("(");
            for (size_t i = 0; i < node->parameterTypes.size(); i++) {
                if (i > 0) emit(", ");
                node->parameterTypes[i]->accept(this);
            }
            emit(")");
            if (node->returnType) {
                emit(", ");
                node->returnType->accept(this);
            }
        }
        emit(">");
    }
    void visit(NullableTypeRef* node) override { node->innerType->accept(this); emit("?"); }
    void visit(RefTypeRef* node) override { emit("ref "); node->innerType->accept(this); }
    void visit(BaseTypeConstraint* node) override { node->baseType->accept(this); }
    void visit(ConstructorConstraint* node) override {
        emit("new(");
        for (size_t i = 0; i < node->parameterTypes.size(); i++) {
            if (i > 0) emit(", ");
            node->parameterTypes[i]->accept(this);
        }
        emit(")");
    }
    void visit(TypeKindConstraint* node) override {
        switch (node->kind) {
            case TypeKindConstraint::Kind::RefType:      emit("ref type"); break;
            case TypeKindConstraint::Kind::ValueType:    emit("value type"); break;
            case TypeKindConstraint::Kind::ArrayType:    emit("array type"); break;
            case TypeKindConstraint::Kind::FunctionType: emit("function type"); break;
        }
    }
    void visit(LiteralPattern* node) override { node->literal->accept(this); }
    void visit(BindingPattern* node) override { if (node->name) node->name->accept(this); else emit("_"); }
    void visit(EnumPattern* node) override {
        emit(".");
        for (size_t i = 0; i < node->path.size(); i++) {
            if (i > 0) emit(".");
            node->path[i]->accept(this);
        }
        if (!node->argumentPatterns.empty()) {
            emit("(");
            for (size_t i = 0; i < node->argumentPatterns.size(); i++) {
                if (i > 0) emit(", ");
                node->argumentPatterns[i]->accept(this);
            }
            emit(")");
        }
    }
    void visit(RangePattern* node) override {
        if (node->start) node->start->accept(this);
        emit(node->isInclusive ? "..=" : "..");
        if (node->end) node->end->accept(this);
    }
    void visit(InPattern* node) override { emit("in "); node->innerPattern->accept(this); }
    void visit(ComparisonPattern* node) override {
        switch (node->op) {
            case ComparisonPattern::Op::Less:         emit("< "); break;
            case ComparisonPattern::Op::Greater:      emit("> "); break;
            case ComparisonPattern::Op::LessEqual:    emit("<= "); break;
            case ComparisonPattern::Op::GreaterEqual: emit(">= "); break;
        }
        node->value->accept(this);
    }
    void visit(CompilationUnit* node) override {
        for (auto* stmt : node->topLevelStatements) {
            if (stmt) {
                stmt->accept(this);
            }
        }
    }
};

} // namespace Myre