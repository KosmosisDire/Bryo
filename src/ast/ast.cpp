#include "ast/ast.hpp"
#include <vector>
#include <stdexcept>

namespace Myre
{
    // --- RTTI System Implementation ---

    // Global registry of all type information, populated by the static initializers.
    static std::vector<AstTypeInfo*> g_all_type_infos;

    // The final, ordered list of type information, indexed by TypeId.
    // This is the definition for the 'extern' variable in ast_rtti.hpp
    std::vector<AstTypeInfo*> g_type_infos;

    AstTypeInfo::AstTypeInfo(const char* name, AstTypeInfo* base_type, AstAcceptFunc accept_func)
    {
        this->name = name;
        this->baseType = base_type;
        this->acceptFunc = accept_func;
        this->typeId = 0; // Will be set by initialize()
        this->fullDerivedCount = 0; // Will be set by initialize()

        if (base_type && base_type != this)
        {
            base_type->derivedTypes.push_back(this);
        }
        g_all_type_infos.push_back(this);
    }

    // Recursive helper to build the ordered list of types for ID assignment.
    static void order_types_recursive(AstTypeInfo* type_info)
    {
        g_type_infos.push_back(type_info);
        for (auto derived_type : type_info->derivedTypes)
        {
            order_types_recursive(derived_type);
        }
    }

    void AstTypeInfo::initialize()
    {
        // Prevent double initialization
        if (!g_type_infos.empty())
        {
            return;
        }

        // The base AstNode must be the root. Find it.
        AstTypeInfo* root = &AstNode::sTypeInfo;

        // Recursively walk the type hierarchy to create a flattened, ordered list.
        g_type_infos.reserve(g_all_type_infos.size());
        order_types_recursive(root);

        // Assign type IDs based on the flattened order.
        for (size_t i = 0; i < g_type_infos.size(); ++i)
        {
            g_type_infos[i]->typeId = static_cast<uint8_t>(i);
        }

        // Calculate the full derived count for each type.
        for (size_t i = 0; i < g_type_infos.size(); ++i)
        {
            AstTypeInfo* current_type = g_type_infos[i];
            uint8_t last_descendant_id = current_type->typeId;

            // Find the highest ID among all descendants.
            std::vector<AstTypeInfo*> worklist = {current_type};
            while(!worklist.empty())
            {
                AstTypeInfo* check_type = worklist.back();
                worklist.pop_back();
                if (check_type->typeId > last_descendant_id)
                {
                    last_descendant_id = check_type->typeId;
                }
                worklist.insert(worklist.end(), check_type->derivedTypes.begin(), check_type->derivedTypes.end());
            }
            current_type->fullDerivedCount = last_descendant_id - current_type->typeId;
        }
    }

    // --- AST Node Method Implementations ---

    void AstNode::init_with_type_id(uint8_t id)
    {
        this->typeId = id;
        this->location = SourceRange();
    }

    void AstNode::accept(StructuralVisitor* visitor)
    {
        assert(!g_type_infos.empty() && "RTTI system not initialized. Call AstTypeInfo::initialize() first.");
        
        // Dispatch to the correct visit method using our RTTI system.
        g_type_infos[this->typeId]->acceptFunc(this, visitor);
    }
    

} // namespace Myre