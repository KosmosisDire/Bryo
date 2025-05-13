#include "script_ast.hpp"

namespace Mycelium::Scripting::Lang
{

AstNode::AstNode() : id(idCounter++) {}

void AstNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const
{
    out << indent << (isLastChild ? "|__ " : "|-- ") << "AstNode (ID: " << id;
    if (location) {
        out << ", Loc: " << location->toString();
    }
    out << ")" << std::endl;
}

void TypeParameterNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const
{
    out << indent << (isLastChild ? "|__ " : "|-- ") << "TypeParameterNode: " << name
        << " (ID: " << id << ")" << std::endl;
}

void DeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const
{
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChild ? "|__ " : "|-- ") << "DeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
}

void UsingDirectiveNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const
{
    out << indent << (isLastChild ? "|__ " : "|-- ") << "UsingDirectiveNode: " << namespaceName
        << " (ID: " << id << ")" << std::endl;
}

void NamespaceDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const
{
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "NamespaceDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_child_list(out, childIndent, true, "Members", members);
}

void CompilationUnitNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const
{
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "CompilationUnitNode";
    if (fileScopedNamespaceName) {
        out << " (FileScopedNamespace: " << *fileScopedNamespaceName << ")";
    }
    out << " (ID: " << id << ")" << std::endl;
    std::string baseChildIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasUsings = !usings.empty();
    bool hasMembers = !members.empty();

    if (hasUsings) {
        print_child_list(out, baseChildIndent, !hasMembers, "Usings", usings);
    }
    if (hasMembers) {
        print_child_list(out, baseChildIndent, true, "TopLevelMembers", members);
    }
}

void TypeDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const
{
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "TypeDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string baseChildIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasTypeParams = !typeParameters.empty();
    bool hasBaseList = !baseList.empty();
    bool hasMembers = !this->members.empty();

    if (hasTypeParams) {
        print_child_list(out, baseChildIndent, !hasBaseList && !hasMembers, "TypeParameters", typeParameters);
    }
    if (hasBaseList) {
        print_child_list(out, baseChildIndent, !hasMembers, "BaseList", baseList);
    }
    if (hasMembers) {
        print_child_list(out, baseChildIndent, true, "Members", this->members);
    }
}

void ClassDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const
{
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ClassDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string baseChildIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasTypeParams = !typeParameters.empty();
    bool hasBaseList = !baseList.empty();
    bool hasMembers = !this->members.empty();

    if (hasTypeParams) {
        print_child_list(out, baseChildIndent, !hasBaseList && !hasMembers, "TypeParameters", typeParameters);
    }
    if (hasBaseList) {
        print_child_list(out, baseChildIndent, !hasMembers, "BaseList", baseList);
    }
    if (hasMembers) {
        print_child_list(out, baseChildIndent, true, "Members", this->members);
    }
}

void StructDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const
{
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "StructDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string baseChildIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasTypeParams = !typeParameters.empty();
    bool hasBaseList = !baseList.empty();
    bool hasMembers = !this->members.empty();

    if (hasTypeParams) {
        print_child_list(out, baseChildIndent, !hasBaseList && !hasMembers, "TypeParameters", typeParameters);
    }
    if (hasBaseList) {
        print_child_list(out, baseChildIndent, !hasMembers, "BaseList", baseList);
    }
    if (hasMembers) {
        print_child_list(out, baseChildIndent, true, "Members", this->members);
    }
}

void MemberDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "MemberDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_optional_child(out, childIndent, true, "Type", type);
}

void FieldDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "FieldDeclarationNode: " << mods;
    if (!declarators.empty() && declarators[0]) {
         out << (mods.empty() ? "" : " ") << declarators[0]->name;
         if (declarators.size() > 1) out << ", ...";
    } else if (!name.empty()) {
        out << (mods.empty() ? "" : " ") << name;
    }
    out << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasType = (this->type && *(this->type)); // type is from MemberDeclarationNode
    bool hasDeclarators = !declarators.empty();

    if (hasType) {
        print_mandatory_child(out, childIndent, !hasDeclarators, "Type", *(this->type));
    }
    if (hasDeclarators) {
        print_child_list(out, childIndent, true, "Declarators", declarators);
    }
}

void MethodDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "MethodDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasReturnType = (this->type && *(this->type));
    bool hasTypeParams = !typeParameters.empty();
    bool hasParams = !parameters.empty();
    bool hasBody = (body && *body);

    if (hasReturnType) {
        print_mandatory_child(out, childIndent, !hasTypeParams && !hasParams && !hasBody, "ReturnType", *(this->type));
    }
    if (hasTypeParams) {
        print_child_list(out, childIndent, !hasParams && !hasBody, "TypeParameters", typeParameters);
    }
    if (hasParams) {
        print_child_list(out, childIndent, !hasBody, "Parameters", parameters);
    }
    if (hasBody) {
        print_mandatory_child(out, childIndent, true, "Body", *body);
    }
}

void ConstructorDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ConstructorDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasParams = !parameters.empty();
    // Body is not optional in AST for constructor
    if (hasParams) {
        print_child_list(out, childIndent, false, "Parameters", parameters); // Body always follows
    }
    print_mandatory_child(out, childIndent, true, "Body", body);
}

void ParameterDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ParameterDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasDefault = (defaultValue && *defaultValue);
    print_mandatory_child(out, childIndent, !hasDefault, "Type", type);

    if (hasDefault) {
        print_mandatory_child(out, childIndent, true, "DefaultValue", *defaultValue);
    }
}

void VariableDeclaratorNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "VariableDeclaratorNode: " << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_optional_child(out, childIndent, true, "Initializer", initializer);
}

void StatementNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const {
    AstNode::print(out, indent, isLastChild);
}

void BlockStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "BlockStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    out << childIndent << (statements.empty() ? "|__ " : "|-- ") << "{" << std::endl; // Visual cue for block
    std::string stmtIndent = childIndent + (statements.empty() ? "    " : "|   ");
    print_child_list(out, stmtIndent, true, "Statements", statements); // Using helper, name is more conceptual
    out << childIndent << "|__ }" << std::endl; // Visual cue for block end
}

void ExpressionStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ExpressionStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_mandatory_child(out, childIndent, true, "Expression", expression);
}

void IfStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "IfStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasElse = (elseStatement && *elseStatement);
    print_mandatory_child(out, childIndent, false, "Condition", condition); // Then always follows
    print_mandatory_child(out, childIndent, !hasElse, "Then", thenStatement);

    if (hasElse) {
        print_mandatory_child(out, childIndent, true, "Else", *elseStatement);
    }
}

void WhileStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "WhileStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_mandatory_child(out, childIndent, false, "Condition", condition);
    print_mandatory_child(out, childIndent, true, "Body", body);
}

void LocalVariableDeclarationStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "LocalVariableDeclarationStatementNode "
        << (isVarDeclaration ? "(var)" : "") << "(ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasDeclarators = !declarators.empty();

    if (isVarDeclaration) {
        out << childIndent << (hasDeclarators ? "|-- " : "|__ ") << "Type: var (implicit)" << std::endl;
    } else {
        print_mandatory_child(out, childIndent, !hasDeclarators, "Type", type);
    }

    if (hasDeclarators) {
        print_child_list(out, childIndent, true, "Declarators", declarators);
    }
}

void ForStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ForStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasDecl = (declaration && *declaration);
    bool hasInits = !initializers.empty();
    bool hasCond = (condition && *condition);
    bool hasIncrs = !incrementors.empty();
    // Body is mandatory in AST for ForStatementNode

    int sections = (hasDecl || hasInits ? 1:0) + (hasCond?1:0) + (hasIncrs?1:0) + 1; // +1 for body
    int currentSection = 0;

    if (hasDecl) {
        currentSection++;
        print_mandatory_child(out, childIndent, currentSection == sections, "Declaration", *declaration);
    } else if (hasInits) {
        currentSection++;
        print_child_list(out, childIndent, currentSection == sections, "Initializers", initializers);
    }

    if (hasCond) {
        currentSection++;
        print_mandatory_child(out, childIndent, currentSection == sections, "Condition", *condition);
    }

    if (hasIncrs) {
        currentSection++;
        print_child_list(out, childIndent, currentSection == sections, "Incrementors", incrementors);
    }

    currentSection++; // For body
    print_mandatory_child(out, childIndent, currentSection == sections, "Body", body);
}

void ForEachStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ForEachStatementNode (VarName: " << variableName << ") (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    print_mandatory_child(out, childIndent, false, "VariableType", variableType);
    print_mandatory_child(out, childIndent, false, "Collection", collection);
    print_mandatory_child(out, childIndent, true, "Body", body);
}

void ReturnStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ReturnStatementNode (ID: " << id << ")" << std::endl;
    if (expression && *expression) {
        std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
        print_mandatory_child(out, childIndent, true, "Expression", *expression);
    }
}

void BreakStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const {
    out << indent << (isLastChild ? "|__ " : "|-- ") << "BreakStatementNode (ID: " << id << ")" << std::endl;
}

void ContinueStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const {
    out << indent << (isLastChild ? "|__ " : "|-- ") << "ContinueStatementNode (ID: " << id << ")" << std::endl;
}

void ExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const {
     AstNode::print(out, indent, isLastChild);
}

void LiteralExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const {
    out << indent << (isLastChild ? "|__ " : "|-- ") << "LiteralExpressionNode: "
        << Mycelium::Scripting::Lang::to_string(kind) << " (\"" << value << "\")"
        << " (ID: " << id << ")" << std::endl;
}

void IdentifierExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const {
    out << indent << (isLastChild ? "|__ " : "|-- ") << "IdentifierExpressionNode: " << name
        << " (ID: " << id << ")" << std::endl;
}

void UnaryExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    bool isPrefix = (op == UnaryOperatorKind::LogicalNot || op == UnaryOperatorKind::UnaryPlus ||
                     op == UnaryOperatorKind::UnaryMinus || op == UnaryOperatorKind::PreIncrement ||
                     op == UnaryOperatorKind::PreDecrement);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "UnaryExpressionNode: "
        << (isPrefix ? Mycelium::Scripting::Lang::to_string(op) + "expr" : "expr" + Mycelium::Scripting::Lang::to_string(op))
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_mandatory_child(out, childIndent, true, "Operand", operand);
}

void BinaryExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "BinaryExpressionNode: (" << Mycelium::Scripting::Lang::to_string(op) << ")"
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_mandatory_child(out, childIndent, false, "Left", left);
    print_mandatory_child(out, childIndent, true, "Right", right);
}

void AssignmentExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "AssignmentExpressionNode: (" << Mycelium::Scripting::Lang::to_string(op) << ")"
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_mandatory_child(out, childIndent, false, "Target", target);
    print_mandatory_child(out, childIndent, true, "Source", source);
}

void MethodCallExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "MethodCallExpressionNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasTarget = (target != nullptr); // target is shared_ptr, not optional
    bool hasTypeArgs = (typeArguments && !typeArguments->empty());
    // arguments is shared_ptr, not optional

    int sections = (hasTarget ? 1:0) + (hasTypeArgs ? 1:0) + 1; // +1 for arguments
    int currentSection = 0;

    if (hasTarget) {
        currentSection++;
        print_mandatory_child(out, childIndent, currentSection == sections, "Target", target);
    }
    if (hasTypeArgs) {
        currentSection++;
        print_child_list(out, childIndent, currentSection == sections, "TypeArguments", *typeArguments);
    }
    
    currentSection++; // For arguments
    print_mandatory_child(out, childIndent, currentSection == sections, "Arguments", arguments);
}

void MemberAccessExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "MemberAccessExpressionNode: ." << memberName
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_mandatory_child(out, childIndent, true, "Target", target);
}

void ObjectCreationExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ObjectCreationExpressionNode (new) (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasArgs = (arguments && *arguments);
    print_mandatory_child(out, childIndent, !hasArgs, "TypeToConstruct", type);

    if (hasArgs) {
        print_mandatory_child(out, childIndent, true, "Arguments", *arguments);
    }
}

void ThisExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const {
    out << indent << (isLastChild ? "|__ " : "|-- ") << "ThisExpressionNode (ID: " << id << ")" << std::endl;
}

void TypeNameNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::stringstream ss;
    ss << name;
    if (!typeArguments.empty()) {
        ss << "<...>";
    }
    if (isArray) {
        ss << "[]";
    }
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "TypeNameNode: " << ss.str()
        << " (ID: " << id << ")" << std::endl;

    if (!typeArguments.empty()) {
        std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
        print_child_list(out, childIndent, true, "TypeArguments", typeArguments);
    }
}

void ArgumentNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ArgumentNode";
    if (name) {
        out << " (" << *name << ":)";
    }
    out << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_mandatory_child(out, childIndent, true, "Expression", expression);
}

void ArgumentListNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ArgumentListNode (" << arguments.size() << " args) (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    print_child_list(out, childIndent, true, "Arguments", arguments);
}

} // namespace Mycelium::Scripting::Lang