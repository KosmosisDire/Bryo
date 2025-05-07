#pragma once

#include <vector>
#include <string>
#include <memory>  
#include <iostream>
#include <optional>

namespace Mycelium::UI::Lang
{
    struct ValueNode; // forward decl
    
    struct AstNode
    {
        virtual ~AstNode() = default;
        virtual void print(int indent = 0) const = 0;
    };

    struct BlockNode : AstNode
    {
        std::string typeIdentifier;                       
        std::vector<std::unique_ptr<ValueNode>> args;
        std::vector<std::unique_ptr<AstNode>> statements;

        BlockNode(std::string typeId) : typeIdentifier(std::move(typeId)) {}

        void print(int indent = 0) const override;
    };

    struct ProgramNode : AstNode
    {
        std::vector<std::unique_ptr<AstNode>> definitions;

        void print(int indent = 0) const override;
    };

    struct ValueNode : AstNode
    {
    };

    struct NumberLiteralNode : ValueNode
    {
        double value;      
        bool isPercentage;

        NumberLiteralNode(double val, bool perc = false) : value(val), isPercentage(perc) {}
        
        void print(int indent = 0) const override;
    };

    struct StringLiteralValueNode : ValueNode
    {
        std::string value;

        StringLiteralValueNode(std::string val) : value(std::move(val)) {}

        void print(int indent = 0) const override;
    };

    struct PropertyNode : AstNode
    {
        std::string name;
        std::unique_ptr<ValueNode> value;

        PropertyNode(std::string n, std::unique_ptr<ValueNode> v) : name(std::move(n)), value(std::move(v)) {}

        void print(int indent = 0) const override;
    };

}
