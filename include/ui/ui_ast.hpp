#pragma once

#include <vector>
#include <string>
#include <memory>  
#include <iostream>
#include <optional>
#include <algorithm>

namespace Mycelium::UI::Lang
{
    struct ValueNode; // forward decl
    struct BlockNode; // forward decl
    
    struct AstNode
    {
        static int idCounter;
        int id;

        AstNode() : id(idCounter++) {}
        virtual ~AstNode() = default;

        virtual std::string toC() const = 0;
    };

    struct StatementNode : AstNode
    {
        std::shared_ptr<BlockNode> parent;
        std::string typeName;
        std::string name;

        StatementNode(std::shared_ptr<BlockNode> parent, std::string typeName, std::string name) : parent(parent), typeName(typeName), name(name) {}
    };

    struct BlockNode : StatementNode
    {            
        std::vector<std::shared_ptr<ValueNode>> args;
        std::vector<std::shared_ptr<AstNode>> statements;

        BlockNode(std::shared_ptr<BlockNode> parent, std::string typeName) : StatementNode(parent, typeName, "")
        {
            auto typeNameCopy = std::string(typeName);
            typeNameCopy[0] = (char)std::tolower(typeNameCopy[0]);
            name = typeNameCopy + std::to_string(id);
        }

        std::string toC() const override;
    };

    struct ProgramNode : AstNode
    {
        std::vector<std::shared_ptr<AstNode>> definitions;

        std::string toC() const override;
    };

    struct ValueNode : AstNode
    {
    };

    struct NumberLiteralNode : ValueNode
    {
        double value;      
        bool isPercentage;

        NumberLiteralNode(double val, bool perc = false) : value(val), isPercentage(perc) {}
        
        std::string toC() const override;
    };

    struct StringLiteralValueNode : ValueNode
    {
        std::string value;

        StringLiteralValueNode(std::string val) : value(val) {}

        std::string toC() const override;
    };

    struct PropertyAssignmentNode : StatementNode
    {
        std::shared_ptr<ValueNode> value;

        PropertyAssignmentNode(std::shared_ptr<BlockNode> parent, std::string name, std::shared_ptr<ValueNode> val) : StatementNode(parent, "PropertyAssignment", name), value(val) {}

        std::string toC() const override;
    };

}
