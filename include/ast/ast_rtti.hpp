#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <assert.h>

namespace Myre
{
    // Forward declarations
    struct AstNode;
    class StructuralVisitor;
    class AstTypeInfo;

    // A function pointer for the visitor's 'accept' method, specific to an AST node type.
    // This allows virtual-like dispatch without the vtable overhead on each node.
    typedef void (*AstAcceptFunc)(AstNode* node, StructuralVisitor* visitor);

    // RTTI metadata for each AST node type.
    // An instance of this class is created statically for each node type.
    class AstTypeInfo
    {
    public:
        const char* name;
        AstTypeInfo* baseType;
        std::vector<AstTypeInfo*> derivedTypes;
        uint8_t typeId;
        uint8_t fullDerivedCount; // The total number of types that inherit from this one.
        AstAcceptFunc acceptFunc;

        AstTypeInfo(const char* name, AstTypeInfo* base_type, AstAcceptFunc accept_func);

        // Initializes the entire RTTI system after all types have been registered.
        // This crucial step calculates the unique typeId and fullDerivedCount for each type.
        static void initialize();
    };
    
    // Make the ordered type info array globally accessible for tools like the AST printer.
    extern std::vector<AstTypeInfo*> g_type_infos;

    // This macro is placed inside the declaration of an AST node class/struct.
    // It injects the necessary static members and helper methods for RTTI.
    #define AST_TYPE(NodeType, BaseType) \
        inline static void class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<NodeType*>(node)); } \
        inline static AstTypeInfo sTypeInfo = AstTypeInfo(#NodeType, &BaseType::sTypeInfo, &NodeType::class_accept); \
        BaseType* to_base() { return static_cast<BaseType*>(this); } \
        NodeType() { init_with_type_id(sTypeInfo.typeId); }

    // Special macro for the root node (AstNode) which has no base type
    #define AST_ROOT_TYPE(NodeType) \
        inline static void class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<NodeType*>(node)); } \
        inline static AstTypeInfo sTypeInfo = AstTypeInfo(#NodeType, nullptr, &NodeType::class_accept); \
        NodeType() { init_with_type_id(sTypeInfo.typeId); }

    // This macro is placed in the corresponding .cpp file to define the static
    // sTypeInfo member for an AST node, effectively registering it with the RTTI system.
    #define AST_DECL_IMPL(NodeType, BaseType) \
        inline void StructuralVisitor::visit(NodeType* node) { visit(static_cast<BaseType*>(node)); }


} // namespace Myre