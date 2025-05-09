#include "script_ast.hpp"

// Note: <iostream> and <sstream> are included via MyceliumLangAst.hpp

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
    for (size_t i = 0; i < members.size(); ++i) {
        if (members[i]) {
            members[i]->print(out, childIndent, i == members.size() - 1);
        }
    }
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
    int sections = (hasUsings ? 1 : 0) + (hasMembers ? 1 : 0) ;
    int currentSection = 0;

    if (hasUsings) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "Usings [" << usings.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < usings.size(); ++i) {
            if (usings[i]) {
                usings[i]->print(out, itemIndent, i == usings.size() - 1);
            }
        }
    }

    if (hasMembers) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "TopLevelMembers [" << members.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < members.size(); ++i) {
            if (members[i]) {
                members[i]->print(out, itemIndent, i == members.size() - 1);
            }
        }
    }
}

void TypeDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const
{
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "TypeDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string baseChildIndent = indent + (isLastChildInParent ? "    " : "|   ");

    int sections = 0;
    if (!typeParameters.empty()) sections++;
    if (!baseList.empty()) sections++;
    if (!this->members.empty()) sections++; 
    
    int currentSection = 0;

    if (!typeParameters.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "TypeParameters [" << typeParameters.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < typeParameters.size(); ++i) {
            if (typeParameters[i]) typeParameters[i]->print(out, itemIndent, i == typeParameters.size() - 1);
        }
    }
    if (!baseList.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "BaseList [" << baseList.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < baseList.size(); ++i) {
            if (baseList[i]) baseList[i]->print(out, itemIndent, i == baseList.size() - 1);
        }
    }
    if (!this->members.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "Members [" << this->members.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < this->members.size(); ++i) {
            if (this->members[i]) this->members[i]->print(out, itemIndent, i == this->members.size() - 1);
        }
    }
}

void ClassDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const
{
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ClassDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string baseChildIndent = indent + (isLastChildInParent ? "    " : "|   ");

    int sections = 0;
    if (!typeParameters.empty()) sections++;
    if (!baseList.empty()) sections++;
    if (!members.empty()) sections++;
    int currentSection = 0;

    if (!typeParameters.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "TypeParameters [" << typeParameters.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < typeParameters.size(); ++i) {
            if (typeParameters[i]) typeParameters[i]->print(out, itemIndent, i == typeParameters.size() - 1);
        }
    }
    if (!baseList.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "BaseList [" << baseList.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < baseList.size(); ++i) {
            if (baseList[i]) baseList[i]->print(out, itemIndent, i == baseList.size() - 1);
        }
    }
    if (!members.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "Members [" << members.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < members.size(); ++i) {
            if (members[i]) members[i]->print(out, itemIndent, i == members.size() - 1);
        }
    }
}

void StructDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const
{
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "StructDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string baseChildIndent = indent + (isLastChildInParent ? "    " : "|   ");
    
    int sections = 0;
    if (!typeParameters.empty()) sections++;
    if (!baseList.empty()) sections++;
    if (!members.empty()) sections++;
    int currentSection = 0;

    if (!typeParameters.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "TypeParameters [" << typeParameters.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < typeParameters.size(); ++i) {
            if (typeParameters[i]) typeParameters[i]->print(out, itemIndent, i == typeParameters.size() - 1);
        }
    }
    if (!baseList.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "BaseList [" << baseList.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < baseList.size(); ++i) {
            if (baseList[i]) baseList[i]->print(out, itemIndent, i == baseList.size() - 1);
        }
    }
    if (!members.empty()) {
        currentSection++;
        bool isLastSection = (currentSection == sections);
        out << baseChildIndent << (isLastSection ? "|__ " : "|-- ") << "Members [" << members.size() << "]:" << std::endl;
        std::string itemIndent = baseChildIndent + (isLastSection ? "    " : "|   ");
        for (size_t i = 0; i < members.size(); ++i) {
            if (members[i]) members[i]->print(out, itemIndent, i == members.size() - 1);
        }
    }
}

void MemberDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "MemberDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    if (type && *type) {
        out << childIndent << "|__ Type:" << std::endl;
        (*type)->print(out, childIndent + "    ", true);
    }
}

void FieldDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "FieldDeclarationNode: " << mods;
    if (!declarators.empty() && declarators[0]) {
         out << (mods.empty() ? "" : " ") << declarators[0]->name; // Primary name for display
         if (declarators.size() > 1) out << ", ...";
    } else if (!name.empty()) { // Fallback to DeclarationNode's name
        out << (mods.empty() ? "" : " ") << name;
    }
    out << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    
    bool hasType = (type && *type);
    bool hasDeclarators = !declarators.empty();
    int sections = (hasType ? 1:0) + (hasDeclarators ? 1:0);
    int currentSection = 0;

    if (hasType) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Type:" << std::endl;
        (*type)->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    }
    if (hasDeclarators) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Declarators [" << declarators.size() << "]:" << std::endl;
        std::string itemIndent = childIndent + (currentSection == sections ? "    " : "|   ");
        for (size_t i = 0; i < declarators.size(); ++i) {
            if (declarators[i]) declarators[i]->print(out, itemIndent, i == declarators.size() - 1);
        }
    }
}

void MethodDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "MethodDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    int sections = 0;
    if (type && *type) sections++;
    if (!typeParameters.empty()) sections++;
    if (!parameters.empty()) sections++;
    if (body && *body) sections++;
    int currentSection = 0;

    if (type && *type) { // Return type
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "ReturnType:" << std::endl;
        (*type)->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    }
    if (!typeParameters.empty()) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "TypeParameters [" << typeParameters.size() << "]:" << std::endl;
        std::string itemIndent = childIndent + (currentSection == sections ? "    " : "|   ");
        for (size_t i = 0; i < typeParameters.size(); ++i) {
            if (typeParameters[i]) typeParameters[i]->print(out, itemIndent, i == typeParameters.size() - 1);
        }
    }
    if (!parameters.empty()) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Parameters [" << parameters.size() << "]:" << std::endl;
        std::string itemIndent = childIndent + (currentSection == sections ? "    " : "|   ");
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (parameters[i]) parameters[i]->print(out, itemIndent, i == parameters.size() - 1);
        }
    }
    if (body && *body) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Body:" << std::endl;
        (*body)->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    }
}

void ConstructorDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers);
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ConstructorDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name // name is class name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasParams = !parameters.empty();
    bool hasBody = (body != nullptr);
    int sections = (hasParams ? 1 : 0) + (hasBody ? 1:0);
    int currentSection = 0;

    if (hasParams) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Parameters [" << parameters.size() << "]:" << std::endl;
        std::string itemIndent = childIndent + (currentSection == sections ? "    " : "|   ");
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (parameters[i]) parameters[i]->print(out, itemIndent, i == parameters.size() - 1);
        }
    }
    if (hasBody) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Body:" << std::endl;
        body->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else { 
        currentSection++; // Should not happen as body is not optional.
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Body: (null)" << std::endl;
    }
}

void ParameterDeclarationNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    std::string mods = modifiers_to_string(modifiers); // Usually empty for params
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ParameterDeclarationNode: " << mods
        << (mods.empty() || name.empty() ? "" : " ") << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    
    bool hasType = (type != nullptr);
    bool hasDefault = (defaultValue && *defaultValue);
    int sections = (hasType?1:0) + (hasDefault?1:0);
    int currentSection = 0;

    if (hasType) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Type:" << std::endl;
        type->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
        currentSection++; // Type is not optional, but pointer could be null
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Type: (null)" << std::endl;
    }

    if (hasDefault) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "DefaultValue:" << std::endl;
        (*defaultValue)->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    }
}

void VariableDeclaratorNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "VariableDeclaratorNode: " << name
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    if (initializer && *initializer) {
        out << childIndent << "|__ Initializer:" << std::endl;
        (*initializer)->print(out, childIndent + "    ", true);
    }
}

void StatementNode::print(std::ostream& out, const std::string& indent, bool isLastChild) const {
    AstNode::print(out, indent, isLastChild); 
}

void BlockStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "BlockStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    out << childIndent << (statements.empty() ? "|__ " : "|-- ") << "{" << std::endl;
    std::string stmtIndent = childIndent + (statements.empty() ? "    " : "|   ");
    for (size_t i = 0; i < statements.size(); ++i) {
        if (statements[i]) {
            statements[i]->print(out, stmtIndent, i == statements.size() - 1);
        } else {
             out << stmtIndent << (i == statements.size() - 1 ? "|__ " : "|-- ") << "(null statement)" << std::endl;
        }
    }
    out << childIndent << "|__ }" << std::endl;
}

void ExpressionStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ExpressionStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    if (expression) {
        expression->print(out, childIndent, true);
    } else {
        out << childIndent << "|__ (null expression)" << std::endl;
    }
}

void IfStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "IfStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    
    bool hasElse = (elseStatement && *elseStatement);
    int sections = 1 + 1 + (hasElse ? 1 : 0); 
    int currentSection = 0;

    currentSection++;
    out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Condition:" << std::endl;
    if (condition) {
        condition->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
        out << childIndent + (currentSection == sections ? "    " : "|   ") << "|__ (null condition)" << std::endl;
    }

    currentSection++;
    out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Then:" << std::endl;
    if (thenStatement) {
        thenStatement->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
         out << childIndent + (currentSection == sections ? "    " : "|   ") << "|__ (null then statement)" << std::endl;
    }
    
    if (hasElse) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Else:" << std::endl;
        (*elseStatement)->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    }
}

void WhileStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "WhileStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    
    out << childIndent << "|-- Condition:" << std::endl;
    if (condition) {
        condition->print(out, childIndent + "|   ", true);
    } else {
         out << childIndent + "|   " << "|__ (null condition)" << std::endl;
    }

    out << childIndent << "|__ Body:" << std::endl;
    if (body) {
        body->print(out, childIndent + "    ", true);
    } else {
        out << childIndent + "    " << "|__ (null body)" << std::endl;
    }
}

void LocalVariableDeclarationStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "LocalVariableDeclarationStatementNode "
        << (isVarDeclaration ? "(var)" : "") << "(ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    bool hasType = (type != nullptr); 
    bool hasDeclarators = !declarators.empty();
    int sections = (hasType || isVarDeclaration ? 1:0) + (hasDeclarators ? 1:0); // If isVar, we imply a "type" section for 'var'
    int currentSection = 0;

    if (isVarDeclaration) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Type: var (implicit)" << std::endl;
    } else if (hasType) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Type:" << std::endl;
        type->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Type: (null/explicitly not var)" << std::endl;
    }

    if (hasDeclarators) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Declarators [" << declarators.size() << "]:" << std::endl;
        std::string itemIndent = childIndent + (currentSection == sections ? "    " : "|   ");
        for (size_t i = 0; i < declarators.size(); ++i) {
            if (declarators[i]) declarators[i]->print(out, itemIndent, i == declarators.size() - 1);
        }
    }
}

void ForStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ForStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    int sections = 0;
    if (declaration && *declaration) sections++;
    else if (!initializers.empty()) sections++;
    if (condition && *condition) sections++;
    if (!incrementors.empty()) sections++;
    if (body) sections++;
    int currentSection = 0;

    if (declaration && *declaration) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Declaration:" << std::endl;
        (*declaration)->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else if (!initializers.empty()) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Initializers [" << initializers.size() << "]:" << std::endl;
        std::string itemIndent = childIndent + (currentSection == sections ? "    " : "|   ");
        for (size_t i = 0; i < initializers.size(); ++i) {
            if (initializers[i]) initializers[i]->print(out, itemIndent, i == initializers.size() - 1);
        }
    }

    if (condition && *condition) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Condition:" << std::endl;
        (*condition)->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    }

    if (!incrementors.empty()) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Incrementors [" << incrementors.size() << "]:" << std::endl;
        std::string itemIndent = childIndent + (currentSection == sections ? "    " : "|   ");
        for (size_t i = 0; i < incrementors.size(); ++i) {
            if (incrementors[i]) incrementors[i]->print(out, itemIndent, i == incrementors.size() - 1);
        }
    }
    
    if (body) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Body:" << std::endl;
        body->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Body: (null)" << std::endl;
    }
}

void ForEachStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ForEachStatementNode (VarName: " << variableName << ") (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    
    int sections = 3; 
    int currentSection = 0;

    currentSection++;
    out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "VariableType:" << std::endl;
    if (variableType) {
         variableType->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
        out << childIndent + (currentSection == sections ? "    " : "|   ") << "|__ (null variable type / implicit)" << std::endl;
    }

    currentSection++;
    out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Collection:" << std::endl;
    if (collection) {
        collection->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
         out << childIndent + (currentSection == sections ? "    " : "|   ") << "|__ (null collection)" << std::endl;
    }

    currentSection++;
    out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Body:" << std::endl;
    if (body) {
        body->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
         out << childIndent + (currentSection == sections ? "    " : "|   ") << "|__ (null body)" << std::endl;
    }
}

void ReturnStatementNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ReturnStatementNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    if (expression && *expression) {
        (*expression)->print(out, childIndent, true);
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
        << Mycelium::Scripting::Lang::to_string(kind) << " (\"" << value << "\")" // Added quotes for string-like value
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
    if (operand) {
        operand->print(out, childIndent, true);
    } else {
        out << childIndent << "|__ (null operand)" << std::endl;
    }
}

void BinaryExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "BinaryExpressionNode: (" << Mycelium::Scripting::Lang::to_string(op) << ")"
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    
    out << childIndent << "|-- Left:" << std::endl;
    if (left) {
         left->print(out, childIndent + "|   ", true);
    } else {
        out << childIndent + "|   " << "|__ (null left operand)" << std::endl;
    }

    out << childIndent << "|__ Right:" << std::endl;
    if (right) {
        right->print(out, childIndent + "    ", true);
    } else {
        out << childIndent + "    " << "|__ (null right operand)" << std::endl;
    }
}

void AssignmentExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "AssignmentExpressionNode: (" << Mycelium::Scripting::Lang::to_string(op) << ")"
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    out << childIndent << "|-- Target:" << std::endl;
    if (target) {
        target->print(out, childIndent + "|   ", true);
    } else {
         out << childIndent + "|   " << "|__ (null target)" << std::endl;
    }

    out << childIndent << "|__ Source:" << std::endl;
    if (source) {
        source->print(out, childIndent + "    ", true);
    } else {
         out << childIndent + "    " << "|__ (null source)" << std::endl;
    }
}

void MethodCallExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "MethodCallExpressionNode (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");

    int sections = 0;
    if (target) sections++;
    if (typeArguments && !typeArguments->empty()) sections++;
    if (arguments) sections++;
    int currentSection = 0;

    if (target) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Target:" << std::endl;
        target->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    }
    if (typeArguments && !typeArguments->empty()) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "TypeArguments [" << typeArguments->size() << "]:" << std::endl;
        std::string itemIndent = childIndent + (currentSection == sections ? "    " : "|   ");
        for (size_t i = 0; i < typeArguments->size(); ++i) {
            if ((*typeArguments)[i]) (*typeArguments)[i]->print(out, itemIndent, i == typeArguments->size() - 1);
        }
    }
    if (arguments) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Arguments:" << std::endl;
        arguments->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Arguments: (null)" << std::endl;
    }
}

void MemberAccessExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "MemberAccessExpressionNode: ." << memberName
        << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    if (target) {
         out << childIndent << "|__ Target:" << std::endl;
        target->print(out, childIndent + "    ", true);
    } else {
        out << childIndent << "|__ Target: (null)" << std::endl;
    }
}

void ObjectCreationExpressionNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ObjectCreationExpressionNode (new) (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    
    bool hasArgs = (arguments && *arguments);
    int sections = 1 + (hasArgs ? 1 : 0); 
    int currentSection = 0;

    currentSection++;
    out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "TypeToConstruct:" << std::endl;
    if (type) {
        type->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
    } else {
        out << childIndent + (currentSection == sections ? "    " : "|   ") << "|__ (null type)" << std::endl;
    }
    
    if (hasArgs) {
        currentSection++;
        out << childIndent << (currentSection == sections ? "|__ " : "|-- ") << "Arguments:" << std::endl;
        (*arguments)->print(out, childIndent + (currentSection == sections ? "    " : "|   "), true);
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
        out << childIndent << "|__ TypeArguments [" << typeArguments.size() << "]:" << std::endl; // This is the only child group
        std::string itemIndent = childIndent + "    ";
        for (size_t i = 0; i < typeArguments.size(); ++i) {
            if (typeArguments[i]) typeArguments[i]->print(out, itemIndent, i == typeArguments.size() - 1);
        }
    }
}

void ArgumentNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ArgumentNode";
    if (name) {
        out << " (" << *name << ":)";
    }
    out << " (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    if (expression) {
        expression->print(out, childIndent, true); // Expression is the only child group
    } else {
         out << childIndent << "|__ (null expression)" << std::endl;
    }
}

void ArgumentListNode::print(std::ostream& out, const std::string& indent, bool isLastChildInParent) const {
    out << indent << (isLastChildInParent ? "|__ " : "|-- ") << "ArgumentListNode (" << arguments.size() << " args) (ID: " << id << ")" << std::endl;
    std::string childIndent = indent + (isLastChildInParent ? "    " : "|   ");
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (arguments[i]) {
            arguments[i]->print(out, childIndent, i == arguments.size() - 1);
        }
    }
}

} // namespace Mycelium::Scripting::Lang