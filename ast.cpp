#include "ast.hpp"
#include "token.hpp"
#include <iostream> // Make sure iostream is included here for std::cout

namespace Mycelium::UI::Lang {

// Helper for indentation
void printIndent(int indent) {
    for (int i = 0; i < indent; ++i) {
        std::cout << "  ";
    }
}

// --- Implementations for print methods ---

void BlockNode::print(int indent) const {
    printIndent(indent);
    std::cout << "Block: " << typeIdentifier;
    if (nameArgument) {
        std::cout << "(\"" << *nameArgument << "\")";
    }
    std::cout << " {" << std::endl;

    for (const auto& stmt : statements) {
        if(stmt) { // Good practice to check pointers
           stmt->print(indent + 1);
        }
    }

    printIndent(indent);
    std::cout << "}" << std::endl;
}


void ProgramNode::print(int indent) const {
    printIndent(indent);
    std::cout << "Program {" << std::endl;
    for (const auto& def : definitions) {
         if(def) {
            def->print(indent + 1);
         }
    }
    printIndent(indent);
    std::cout << "}" << std::endl;
}

void NumberLiteralNode::print(int indent) const
{
    printIndent(indent);
    std::cout << "Number: " << value << (isPercentage ? "%" : "") << std::endl;
}

void StringLiteralValueNode::print(int indent) const
{
    printIndent(indent);
    std::cout << "String: \"" << value << "\"" << std::endl;
}

void PropertyNode::print(int indent) const
{
    printIndent(indent);
    std::cout << "Property: " << name << ": ";
    if (value) value->print(0); // Print value inline or on new line
    else std::cout << "[no value]" << std::endl;
}


} // namespace Mycelium::UI::Lang