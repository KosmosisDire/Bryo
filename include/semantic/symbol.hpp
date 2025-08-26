#pragma once

#include <string>
#include <memory>
#include <vector>
#include "scope.hpp"
#include "type.hpp"
#include "conversions.hpp"

namespace Bryo
{

    // Forward declarations
    struct ExpressionNode;
    struct BlockStatementNode;
    class SymbolTable;

    // Access levels
    enum class AccessLevel
    {
        Public,
        Private,
        Protected
    };

    // Symbol modifiers
    enum class SymbolModifiers : uint32_t
    {
        None = 0,
        Static = 1 << 0,
        Virtual = 1 << 1,
        Override = 1 << 2,
        Abstract = 1 << 3,
        Async = 1 << 4,
        Extern = 1 << 5,
        Enforced = 1 << 6,
        Ref = 1 << 7,
        Inline = 1 << 8
    };

    inline SymbolModifiers operator|(SymbolModifiers a, SymbolModifiers b)
    {
        return static_cast<SymbolModifiers>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline SymbolModifiers operator&(SymbolModifiers a, SymbolModifiers b)
    {
        return static_cast<SymbolModifiers>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    // Base symbol class - all symbols are scope nodes
    class Symbol : public ScopeNode
    {
    protected:
        std::string name_;
        AccessLevel access_ = AccessLevel::Private;
        SymbolModifiers modifiers_ = SymbolModifiers::None;

    public:
        // Basic properties
        const std::string &name() const { return name_; }
        AccessLevel access() const { return access_; }
        SymbolModifiers modifiers() const { return modifiers_; }

        void set_name(const std::string &name) { name_ = name; }
        void set_access(AccessLevel access) { access_ = access; }
        void add_modifier(SymbolModifiers mod) { modifiers_ = modifiers_ | mod; }

        bool has_modifier(SymbolModifiers mod) const
        {
            return (modifiers_ & mod) != SymbolModifiers::None;
        }

        // For debugging/display
        virtual const char *kind_name() const = 0;

        // Build qualified name
        std::string get_qualified_name() const;
    };

    class UnscopedSymbol : public Symbol
    {
    };

    class ScopedSymbol : public Symbol, public Scope
    {
    public:
        // Implement Scope interface
        ScopeNode *as_scope_node() override { return this; }
        const ScopeNode *as_scope_node() const override { return this; }
    };

    // Base for types and enums
    class TypeLikeSymbol : public ScopedSymbol
    {
    public:
        // Can be used as a type in declarations
    };

    // Base for symbols that have a type (variables, parameters, fields, properties, functions)
    class TypedSymbol
    {
    protected:
        TypePtr type_;

    public:
        // Abstract class - cannot be instantiated directly
        virtual ~TypedSymbol() = default;

        // Type access
        virtual TypePtr type() const { return type_; }
        virtual void set_type(TypePtr type) { type_ = type; }

        // Virtual function to get back to the ScopeNode (if this is also a ScopeNode)
        virtual ScopeNode *as_scope_node() = 0;
        virtual const ScopeNode *as_scope_node() const = 0;

        // Proxy the as() functions through the ScopeNode
        template <typename T>
        T *as()
        {
            if (auto node = as_scope_node())
            {
                return node->as<T>();
            }
            return nullptr;
        }

        template <typename T>
        const T *as() const
        {
            if (auto node = as_scope_node())
            {
                return node->as<T>();
            }
            return nullptr;
        }

        template <typename T>
        bool is() const
        {
            if (auto node = as_scope_node())
            {
                return node->is<T>();
            }
            return false;
        }
    };

    class ScopedTypedSymbol : public ScopedSymbol, public TypedSymbol
    {
    public:
        // Implement TypedSymbol's virtual function
        ScopeNode *as_scope_node() override { return this; }
        const ScopeNode *as_scope_node() const override { return this; }

        // Resolve ambiguity by using ScopeNode's as() functions
        using ScopeNode::as;
        using ScopeNode::is;
    };

    class UnscopedTypedSymbol : public UnscopedSymbol, public TypedSymbol
    {
    public:
        // Implement TypedSymbol's virtual function
        ScopeNode *as_scope_node() override { return this; }
        const ScopeNode *as_scope_node() const override { return this; }

        // Resolve ambiguity by using ScopeNode's as() functions
        using ScopeNode::as;
        using ScopeNode::is;
    };

    // Regular type (class/struct)
    class TypeSymbol : public TypeLikeSymbol
    {
    public:
        const char *kind_name() const override { return "type"; }

        bool is_ref_type() const { return has_modifier(SymbolModifiers::Ref); }
        bool is_abstract() const { return has_modifier(SymbolModifiers::Abstract); }
    };

    // Enum type
    class EnumSymbol : public TypeLikeSymbol
    {
    public:
        const char *kind_name() const override { return "enum"; }
    };

    // Namespace
    class NamespaceSymbol : public ScopedSymbol
    {
    public:
        const char *kind_name() const override { return "namespace"; }
    };

    // Variable: used for both local and member variables
    class VariableSymbol : public UnscopedTypedSymbol
    {
    public:
        bool is_field = false;
        const char *kind_name() const override { return is_field ? "field" : "var"; }
    };

    // Parameter
    class ParameterSymbol : public UnscopedTypedSymbol
    {
    public:
        const char *kind_name() const override { return "param"; }
    };

    // Property
    class PropertySymbol : public ScopedTypedSymbol
    {
    public:
        const char *kind_name() const override { return "prop"; }
        bool has_getter() const { return const_cast<PropertySymbol *>(this)->lookup_local(std::string("get")) != nullptr; }
        bool has_setter() const { return const_cast<PropertySymbol *>(this)->lookup_local(std::string("set")) != nullptr; }
    };

    // Function
    class FunctionSymbol : public ScopedTypedSymbol
    {
        std::vector<ParameterSymbol *> parameters_;

    public:
        const char *kind_name() const override { return "fn"; }

        // Override to use the inherited type_ as return type
        void set_return_type(TypePtr type) { type_ = type; }
        void set_parameters(std::vector<ParameterSymbol *> params)
        {
            parameters_ = std::move(params);
        }

        TypePtr return_type() const { return type_; }
        const std::vector<ParameterSymbol *> &parameters() const { return parameters_; }

        std::string get_mangled_name() const
        {
            std::string mangled = get_qualified_name();
            mangled += "_";

            // Add return type
            if (type_)
            {
                mangled += type_->get_name();
            }
            mangled += "_";

            for (size_t i = 0; i < parameters_.size(); ++i)
            {
                if (i > 0)
                    mangled += "_";
                if (parameters_[i]->type())
                {
                    mangled += parameters_[i]->type()->get_name();
                }
            }
            return mangled;
        }

        std::string full_signature() const
        {
            std::string sig = name_ + "(";
            for (size_t i = 0; i < parameters_.size(); ++i)
            {
                if (i > 0)
                    sig += ", ";
                if (parameters_[i]->type())
                {
                    sig += parameters_[i]->type()->get_name();
                }
                else
                {
                    sig += "var";
                }
            }
            sig += "): ";
            if (type_)
            {
                sig += type_->get_name();
            }
            return sig;
        }
    };

    class FunctionGroupSymbol : public Symbol
    {
        public:

        std::vector<std::unique_ptr<FunctionSymbol>> local_overloads;
        const char *kind_name() const override { return "fn group"; }

        // Add a new overload to the group
        // returns false if there is a conflict
        void add_overload(std::unique_ptr<FunctionSymbol> func)
        {
            func->parent = this; // Set parent to the group
            local_overloads.push_back(std::move(func));
        }

        std::vector<FunctionSymbol *> get_overloads() const
        {
            std::vector<FunctionSymbol *> result;
            for (auto &func : local_overloads)
            {
                result.push_back(func.get());
            }
            return result;
        }
    };

    // Enum case
    class EnumCaseSymbol : public UnscopedSymbol
    {
        std::vector<TypePtr> parameters_;

    public:
        const char *kind_name() const override { return "enum_case"; }

        void set_params(std::vector<TypePtr> types) { parameters_ = std::move(types); }
        const std::vector<TypePtr> &params() const { return parameters_; }

        bool is_simple() const { return parameters_.empty(); }
        bool is_tagged() const { return !parameters_.empty(); }
    };

} // namespace Bryo