#pragma once

#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_rtti.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <sstream>

using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Scripting::Common;
using namespace Mycelium::Scripting;
using namespace Mycelium;

// A code-like visitor to print AST nodes as pseudo-code
class AstPrinterVisitor : public StructuralVisitor
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

    void print_modifiers(const SizedArray<ModifierKind>& modifiers) {
        for (int i = 0; i < modifiers.size; i++) {
            output << to_string(modifiers[i]) << " ";
        }
    }

    std::string get_node_content() {
        std::string result = output.str();
        output.str(""); // Clear the stream
        output.clear();
        return result;
    }

public:
    // --- Base Node Types ---
    
    void visit(AstNode* node) override {
        print_line("[UnhandledNode: " + std::string(g_ordered_type_infos[node->typeId]->name) + "]");
    }

    void visit(TokenNode* node) override {
        print_inline("\"" + std::string(node->text) + "\"");
    }

    void visit(IdentifierNode* node) override {
        print_inline(std::string(node->name));
    }

    void visit(ErrorNode* node) override {
        print_line("[ERROR: " + node->error_message + "]");
    }

    // --- Expression Base ---
    
    void visit(ExpressionNode* node) override {
        print_line("[AbstractExpression]");
    }

    // --- Expression Implementations ---

    void visit(LiteralExpressionNode* node) override {
        print_inline(std::string(node->token->text));
    }

    void visit(IdentifierExpressionNode* node) override {
        print_inline(std::string(node->identifier->name));
    }

    void visit(ParenthesizedExpressionNode* node) override {
        print_inline("(");
        if (node->expression) {
            node->expression->accept(this);
        }
        print_inline(")");
    }

    void visit(UnaryExpressionNode* node) override {
        if (node->isPostfix) {
            if (node->operand) {
                node->operand->accept(this);
            }
            print_inline(std::string(to_string(node->opKind)));
        } else {
            print_inline(std::string(to_string(node->opKind)));
            if (node->operand) {
                node->operand->accept(this);
            }
        }
    }

    void visit(BinaryExpressionNode* node) override {
        if (node->left) {
            node->left->accept(this);
        } else {
            print_inline("[missing-left]");
        }
        print_inline(" " + std::string(to_string(node->opKind)) + " ");
        if (node->right) {
            node->right->accept(this);
        } else {
            print_inline("[missing-right]");
        }
    }

    void visit(AssignmentExpressionNode* node) override {
        if (node->target) {
            node->target->accept(this);
        }
        print_inline(" " + std::string(to_string(node->opKind)) + " ");
        if (node->source) {
            node->source->accept(this);
        }
    }

    void visit(CallExpressionNode* node) override {
        if (node->target) {
            node->target->accept(this);
        }
        print_inline("(");
        for (int i = 0; i < node->arguments.size; i++) {
            if (i > 0) print_inline(", ");
            if (node->arguments[i]) {
                node->arguments[i]->accept(this);
            } else {
                print_inline("[missing-arg]");
            }
        }
        print_inline(")");
    }

    void visit(MemberAccessExpressionNode* node) override {
        if (node->target) {
            node->target->accept(this);
        }
        print_inline(".");
        if (node->member) {
            print_inline(std::string(node->member->name));
        }
    }

    void visit(NewExpressionNode* node) override {
        print_inline("new ");
        if (node->type) {
            node->type->accept(this);
        }
        if (node->constructorCall) {
            // Print just the arguments part since type is already printed
            print_inline("(");
            for (int i = 0; i < node->constructorCall->arguments.size; i++) {
                if (i > 0) print_inline(", ");
                if (node->constructorCall->arguments[i]) {
                    node->constructorCall->arguments[i]->accept(this);
                }
            }
            print_inline(")");
        }
    }

    void visit(ThisExpressionNode* node) override {
        print_inline("this");
    }

    void visit(CastExpressionNode* node) override {
        print_inline("(");
        if (node->targetType) {
            node->targetType->accept(this);
        }
        print_inline(")");
        if (node->expression) {
            node->expression->accept(this);
        }
    }

    void visit(IndexerExpressionNode* node) override {
        if (node->target) {
            node->target->accept(this);
        }
        print_inline("[");
        if (node->index) {
            node->index->accept(this);
        }
        print_inline("]");
    }

    void visit(TypeOfExpressionNode* node) override {
        print_inline("typeof(");
        if (node->type) {
            node->type->accept(this);
        }
        print_inline(")");
    }

    void visit(SizeOfExpressionNode* node) override {
        print_inline("sizeof(");
        if (node->type) {
            node->type->accept(this);
        }
        print_inline(")");
    }

    void visit(MatchExpressionNode* node) override {
        print_inline("match (");
        if (node->expression) {
            node->expression->accept(this);
        }
        print_inline(") {");
        
        if (!node->arms.empty()) {
            print_line("");
            indentLevel++;
            for (auto arm : node->arms) {
                if (arm) {
                    arm->accept(this);
                }
            }
            indentLevel--;
            print_line(get_indent() + "}");
        } else {
            print_inline(" }");
        }
    }

    void visit(ConditionalExpressionNode* node) override {
        if (node->condition) {
            node->condition->accept(this);
        }
        print_inline(" ? ");
        if (node->whenTrue) {
            node->whenTrue->accept(this);
        }
        print_inline(" : ");
        if (node->whenFalse) {
            node->whenFalse->accept(this);
        }
    }

    void visit(RangeExpressionNode* node) override {
        if (node->start) {
            node->start->accept(this);
        }
        if (node->rangeOp) {
            print_inline(std::string(node->rangeOp->text));
        } else {
            print_inline("..");
        }
        if (node->end) {
            node->end->accept(this);
        }
    }

    void visit(EnumMemberExpressionNode* node) override {
        print_inline(".");
        if (node->memberName) {
            print_inline(std::string(node->memberName->name));
        }
    }

    void visit(FieldKeywordExpressionNode* node) override {
        print_inline("field");
    }

    void visit(ValueKeywordExpressionNode* node) override {
        print_inline("value");
    }

    // --- Statement Base ---
    
    void visit(StatementNode* node) override {
        print_line("[AbstractStatement]");
    }

    // --- Statement Implementations ---

    void visit(EmptyStatementNode* node) override {
        print_line(";");
    }

    void visit(BlockStatementNode* node) override {
        print_line("{");
        indentLevel++;
        for (auto stmt : node->statements) {
            if (stmt) {
                stmt->accept(this);
                print_line(get_node_content());
            }
        }
        indentLevel--;
        print_line("}");
    }

    void visit(ExpressionStatementNode* node) override {
        if (node->expression) {
            node->expression->accept(this);
        }
        print_inline(";");
        print_line(get_node_content());
    }

    void visit(IfStatementNode* node) override {
        print_inline("if (");
        if (node->condition) {
            node->condition->accept(this);
        }
        print_inline(") ");
        
        std::string condition_part = get_node_content();
        print_line(condition_part);
        
        indentLevel++;
        if (node->thenStatement) {
            node->thenStatement->accept(this);
        }
        indentLevel--;
        
        if (node->elseStatement) {
            print_line(get_indent() + "else");
            indentLevel++;
            node->elseStatement->accept(this);
            indentLevel--;
        }
    }

    void visit(WhileStatementNode* node) override {
        print_inline("while (");
        if (node->condition) {
            node->condition->accept(this);
        }
        print_inline(") ");
        
        std::string condition_part = get_node_content();
        print_line(condition_part);
        
        indentLevel++;
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
    }

    void visit(ForStatementNode* node) override {
        print_inline("for (");
        
        // Initializer
        if (node->initializer) {
            node->initializer->accept(this);
            print_inline(" ");
        }
        else
        {
            print_inline("; ");
        }
        
        // Condition
        if (node->condition) {
            node->condition->accept(this);
        }
        print_inline("; ");
        
        // Incrementors
        for (int i = 0; i < node->incrementors.size; i++) {
            if (i > 0) print_inline(", ");
            if (node->incrementors[i]) {
                node->incrementors[i]->accept(this);
            }
        }
        print_inline(") ");
        
        std::string header = get_node_content();
        print_line(header);
        
        if (node->body) {
            node->body->accept(this);
        }
    }

    void visit(ForInStatementNode* node) override {
        print_inline("for (");
        if (node->mainVariable) {
            node->mainVariable->accept(this);
        }
        print_inline(" in ");
        if (node->iterable) {
            node->iterable->accept(this);
        }
        print_inline(") ");
        
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        if (node->body) {
            node->body->accept(this);
        }
        indentLevel--;
    }

    void visit(ReturnStatementNode* node) override {
        print_inline("return");
        if (node->expression) {
            print_inline(" ");
            node->expression->accept(this);
        }
        print_inline(";");
        print_line(get_node_content());
    }

    void visit(BreakStatementNode* node) override {
        print_line("break;");
    }

    void visit(ContinueStatementNode* node) override {
        print_line("continue;");
    }

    void visit(VariableDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        
        if (node->varKeyword) {
            print_inline("var ");
        }
        else if (node->type) {
            node->type->accept(this);
            print_inline(" ");
        }
        else
        {
            print_inline("[Print Error: Missing var/type]");
        }
        
        // Print names
        if (node->names.size > 0) {
            for (int i = 0; i < node->names.size; i++) {
                if (i > 0) print_inline(", ");
                if (node->names[i]) {
                    print_inline(std::string(node->names[i]->name));
                }
            }
        } else if (node->name) {
            print_inline(std::string(node->name->name));
        }
        
        // Initializer
        if (node->initializer) {
            print_inline(" = ");
            node->initializer->accept(this);
        } 
        
        print_inline(";");
    }

    // --- Declaration Base ---
    
    void visit(DeclarationNode* node) override {
        print_modifiers(node->modifiers);
        std::string name = node->name ? std::string(node->name->name) : "[unnamed]";
        print_line("[AbstractDeclaration: " + name + "]");
    }

    // --- Declaration Implementations ---

    void visit(NamespaceDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        print_inline("namespace ");
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
        print_inline(" ");
        
        std::string header = get_node_content();
        print_line(header);
        
        if (node->body) {
            node->body->accept(this);
        }
    }

    void visit(UsingDirectiveNode* node) override {
        print_inline("using ");
        if (node->namespaceName) {
            node->namespaceName->accept(this);
        }
        print_inline(";");
        print_line(get_node_content());
    }

    void visit(TypeDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        print_inline("type ");
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
        print_inline(" {");
        
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        for (auto member : node->members) {
            if (member) {
                member->accept(this);
                print_line(get_node_content());
            }
        }
        indentLevel--;
        print_line("}");
    }

    void visit(InterfaceDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        print_inline("interface ");
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
        print_inline(" {");
        
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        for (auto member : node->members) {
            if (member) {
                member->accept(this);
                print_line(get_node_content());
            }
        }
        indentLevel--;
        print_line("}");
    }

    void visit(EnumDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        print_inline("enum ");
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
        print_inline(" {");
        
        std::string header = get_node_content();
        print_line(header);
        
        indentLevel++;
        
        // Print cases
        for (auto enumCase : node->cases) {
            if (enumCase) {
                enumCase->accept(this);
                print_line(get_node_content());
            }
        }
        
        // Print methods
        for (auto method : node->methods) {
            if (method) {
                method->accept(this);
                print_line(get_node_content());
            }
        }
        
        indentLevel--;
        print_line("}");
    }

    void visit(MemberDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        std::string name = node->name ? std::string(node->name->name) : "[unnamed]";
        print_line("[AbstractMember: " + name + "]");
    }


    void visit(FunctionDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        print_inline("fn ");
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
        
        print_inline("(");
        for (int i = 0; i < node->parameters.size; i++) {
            if (i > 0) print_inline(", ");
            if (node->parameters[i]) {
                node->parameters[i]->accept(this);
                get_node_content(); // Clear since we're building inline
            }
        }
        print_inline(")");
        
        // Return type
        if (node->returnType) {
            print_inline(": ");
            node->returnType->accept(this);
        }
        
        std::string signature = get_node_content();
        
        if (node->body) {
            print_line(signature + " ");
            node->body->accept(this);
        } else {
            print_line(signature + ";");
        }
    }

    void visit(ParameterNode* node) override {
        print_modifiers(node->modifiers);
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
        if (node->type) {
            print_inline(": ");
            node->type->accept(this);
        }
        if (node->defaultValue) {
            print_inline(" = ");
            node->defaultValue->accept(this);
        }
    }

    void visit(GenericParameterNode* node) override {
        print_modifiers(node->modifiers);
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
    }

    void visit(PropertyDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
        if (node->type) {
            print_inline(": ");
            node->type->accept(this);
        }
        
        if (node->getterExpression) {
            print_inline(" => ");
            node->getterExpression->accept(this);
            print_inline(";");
        } else if (!node->accessors.empty()) {
            print_inline(" {");
            std::string header = get_node_content();
            print_line(header);
            
            indentLevel++;
            for (auto accessor : node->accessors) {
                if (accessor) {
                    accessor->accept(this);
                    print_line(get_node_content());
                }
            }
            indentLevel--;
            print_line("}");
        } else {
            print_inline(";");
        }
    }

    void visit(PropertyAccessorNode* node) override {
        print_modifiers(node->modifiers);
        if (node->accessorKeyword) {
            print_inline(std::string(node->accessorKeyword->text));
        }
        
        if (node->expression) {
            print_inline(" => ");
            node->expression->accept(this);
            print_inline(";");
        } else if (node->body) {
            print_inline(" ");
            std::string header = get_node_content();
            print_line(header);
            node->body->accept(this);
        } else {
            print_inline(";");
        }
    }

    void visit(ConstructorDeclarationNode* node) override {
        print_modifiers(node->modifiers);
        print_inline("new(");
        for (int i = 0; i < node->parameters.size; i++) {
            if (i > 0) print_inline(", ");
            if (node->parameters[i]) {
                node->parameters[i]->accept(this);
                get_node_content(); // Clear inline content
            }
        }
        print_inline(") ");
        
        std::string signature = get_node_content();
        print_line(signature);
        
        if (node->body) {
            node->body->accept(this);
        }
    }

    void visit(EnumCaseNode* node) override {
        print_modifiers(node->modifiers);
        print_inline("case ");
        if (node->name) {
            print_inline(std::string(node->name->name));
        }
        
        if (!node->associatedData.empty()) {
            print_inline("(");
            for (int i = 0; i < node->associatedData.size; i++) {
                if (i > 0) print_inline(", ");
                if (node->associatedData[i]) {
                    node->associatedData[i]->accept(this);
                    get_node_content(); // Clear inline content
                }
            }
            print_inline(")");
        }
        
        print_inline(";");
    }

    // --- Match Pattern Implementations ---

    void visit(MatchArmNode* node) override {
        if (node->pattern) {
            node->pattern->accept(this);
        }
        print_inline(" => ");
        if (node->result) {
            node->result->accept(this);
        }
        print_inline(",");
        
        std::string arm = get_node_content();
        print_line(arm);
    }

    void visit(MatchPatternNode* node) override {
        print_inline("[AbstractPattern]");
    }

    void visit(EnumPatternNode* node) override {
        print_inline(".");
        if (node->enumCase) {
            print_inline(std::string(node->enumCase->name));
        }
    }

    void visit(RangePatternNode* node) override {
        if (node->start) {
            node->start->accept(this);
        }
        if (node->rangeOp) {
            print_inline(std::string(node->rangeOp->text));
        } else {
            print_inline("..");
        }
        if (node->end) {
            node->end->accept(this);
        }
    }

    void visit(ComparisonPatternNode* node) override {
        if (node->comparisonOp) {
            print_inline(std::string(node->comparisonOp->text));
        }
        print_inline(" ");
        if (node->value) {
            node->value->accept(this);
        }
    }

    void visit(WildcardPatternNode* node) override {
        print_inline("_");
    }

    void visit(LiteralPatternNode* node) override {
        if (node->literal) {
            node->literal->accept(this);
        }
    }

    // --- Type Name Implementations ---

    void visit(TypeNameNode* node) override {
        if (node->identifier) {
            print_inline(std::string(node->identifier->name));
        }
    }

    void visit(QualifiedTypeNameNode* node) override {
        if (node->left) {
            node->left->accept(this);
        }
        print_inline(".");
        if (node->right) {
            print_inline(std::string(node->right->name));
        }
    }

    void visit(ArrayTypeNameNode* node) override {
        if (node->elementType) {
            node->elementType->accept(this);
        }
        print_inline("[]");
    }

    void visit(GenericTypeNameNode* node) override {
        if (node->baseType) {
            node->baseType->accept(this);
        }
        print_inline("<");
        for (int i = 0; i < node->arguments.size; i++) {
            if (i > 0) print_inline(", ");
            if (node->arguments[i]) {
                node->arguments[i]->accept(this);
            }
        }
        print_inline(">");
    }

    // --- Root ---
    
    void visit(CompilationUnitNode* node) override {
        print_line("// Compilation Unit");
        for (auto stmt : node->statements) {
            if (stmt) {
                stmt->accept(this);
                print_line(get_node_content());
            }
        }
    }
};