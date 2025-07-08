#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace Mycelium::Scripting::Lang
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
    extern std::vector<AstTypeInfo*> g_ordered_type_infos;


    // This macro is placed inside the declaration of an AST node class/struct.
    // It injects the necessary static members and helper methods for RTTI.
    #define AST_TYPE(NodeType, BaseType) \
        static AstTypeInfo sTypeInfo; \
        static void class_accept(AstNode* node, StructuralVisitor* visitor); \
        BaseType* to_base() { return static_cast<BaseType*>(this); } \
        NodeType() { init_with_type_id(sTypeInfo.typeId); }

    // Special macro for the root node (AstNode) which has no base type
    #define AST_ROOT_TYPE(NodeType) \
        static AstTypeInfo sTypeInfo; \
        static void class_accept(AstNode* node, StructuralVisitor* visitor); \
        NodeType() { init_with_type_id(sTypeInfo.typeId); }

    // This macro is placed in the corresponding .cpp file to define the static
    // sTypeInfo member for an AST node, effectively registering it with the RTTI system.
    #define AST_DECL_IMPL(NodeType, BaseType) \
        AstTypeInfo NodeType::sTypeInfo(#NodeType, &BaseType::sTypeInfo, &NodeType::class_accept);

    // Special macro for the root node (AstNode) which has no base type
    #define AST_DECL_ROOT_IMPL(NodeType) \
        AstTypeInfo NodeType::sTypeInfo(#NodeType, nullptr, &NodeType::class_accept);

    // --- RTTI Helper Functions ---

    // Checks if a node is of type T or a derived type of T.
    template <typename T>
    bool node_is(AstNode* node);

    // Checks if a node is exactly of type T.
    template <typename T>
    bool node_is_exact(AstNode* node);

    // Safely casts a node to type T if it's an instance of T or a derived type.
    // Returns nullptr otherwise.
    template <typename T>
    T* node_cast(AstNode* node);

    // Safely casts a node to type T only if it's an exact instance of T.
    // Returns nullptr otherwise.
    template <typename T>
    T* node_cast_exact(AstNode* node);

    // Get human-readable type name from AST node
    const char* get_node_type_name(const AstNode* node);

    // Get human-readable type name from type ID
    const char* get_type_name_from_id(uint8_t type_id);

} // namespace Mycelium::Scripting::Lang