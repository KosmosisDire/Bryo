#include "ui_ast.hpp"
#include "ui_token.hpp"
#include <iostream>

namespace Mycelium::UI::Lang
{
    std::string BlockNode::toC() const
    {
        std::string cppCode = typeName + " " + name;
        // cppCode += "(";
        // for (size_t i = 0; i < args.size(); ++i)
        // {
        //     if (i > 0)
        //     {
        //         cppCode += ", ";
        //     }
        //     cppCode += args[i]->toC();
        // }
        // cppCode += ");\n";
        cppCode += ";\n";
        cppCode += "mui_begin(&" + name + ");\n";
        for (const auto &stmt : statements)
        {
            if (stmt)
            {
                cppCode += stmt->toC() + "\n";
            }
        }
        cppCode += "mui_end();\n";
        return cppCode;
    }

    std::string ProgramNode::toC() const
    {
        std::string cppCode = "int main()\n{\n";
        for (const auto &def : definitions)
        {
            if (def)
            {
                cppCode += def->toC() + "\n";
            }
        }
        cppCode += "return 0;\n";
        cppCode += "}\n";
        return cppCode;
    }

    std::string NumberLiteralNode::toC() const
    {
        return isPercentage ? std::to_string(value / 100.0f) + "f" : std::to_string(value);
    }

    std::string StringLiteralValueNode::toC() const
    {
        return "\"" + value + "\"";
    }

    std::string PropertyAssignmentNode::toC() const
    {
        return parent->name + "." + name + " = " + (value ? value->toC() : "0") + ";";
    }
    int AstNode::idCounter = 0;

}