#include "ast.hpp"
#include "token.hpp"
#include <iostream>

namespace Mycelium::UI::Lang
{

    void printIndent(int indent)
    {
        for (int i = 0; i < indent; ++i)
        {
            std::cout << "  ";
        }
    }

    void BlockNode::print(int indent) const
    {
        printIndent(indent);
        std::cout << "Block: " << typeIdentifier;
        if (args.size() > 0)
        {
            std::cout << "(";
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (i > 0)
                {
                    std::cout << ", ";
                }
                args[i]->print(0);
            }
            std::cout << ")";
        }

        std::cout << " {" << std::endl;

        for (const auto &stmt : statements)
        {
            if (stmt)
            {
                stmt->print(indent + 1);
            }
        }

        printIndent(indent);
        std::cout << "}" << std::endl;
    }

    void ProgramNode::print(int indent) const
    {
        printIndent(indent);
        std::cout << "Program {" << std::endl;
        for (const auto &def : definitions)
        {
            if (def)
            {
                def->print(indent + 1);
            }
        }
        printIndent(indent);
        std::cout << "}" << std::endl;
    }

    void NumberLiteralNode::print(int indent) const
    {
        printIndent(indent);
        std::cout << "Number: " << value << (isPercentage ? "%" : "");
    }

    void StringLiteralValueNode::print(int indent) const
    {
        printIndent(indent);
        std::cout << "String: \"" << value << "\"";
    }

    void PropertyNode::print(int indent) const
    {
        printIndent(indent);
        std::cout << name << " = ";
        if (value)
        {
            value->print(0);
        }
        else
        {
            std::cout << "[no value]";
        }
        std::cout << ";" << std::endl;
    }

}