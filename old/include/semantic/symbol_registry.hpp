#pragma once

#include "semantic/type_system.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <optional>
#include <stdexcept>

namespace Myre
{

    // === SOURCE LOCATION ===

    struct SourceLocation
    {
        int line = 0;
        int column = 0;
        std::string filename;

        SourceLocation() = default;
        SourceLocation(int l, int c, std::string f = "")
            : line(l), column(c), filename(std::move(f)) {}

        std::string to_string() const
        {
            return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
        }
    };

    // === IMMUTABLE SYMBOL ===

    class Symbol
    {
    public:
        enum class Kind
        {
            Variable,
            Function,
            Type,
            Parameter
        };

    private:
        std::string name_;
        Kind kind_;
        std::shared_ptr<Type> type_;
        SourceLocation location_;

    public:
        // Default constructor for container usage
        Symbol() : name_(""), kind_(Kind::Variable), type_(nullptr), location_() {}

        Symbol(std::string name, Kind kind,
               std::shared_ptr<Type> type, SourceLocation location)
            : name_(std::move(name)), kind_(kind), type_(std::move(type)), location_(location) {}

        // Immutable getters
        const std::string &name() const { return name_; }
        Kind kind() const { return kind_; }
        const Type &type() const
        {
            if (!type_)
            {
                throw std::runtime_error("Symbol has null type - check default constructor usage");
            }
            return *type_;
        }
        std::shared_ptr<Type> type_ptr() const { return type_; }
        const SourceLocation &location() const { return location_; }

        std::string kind_string() const
        {
            switch (kind_)
            {
            case Kind::Variable:
                return "Variable";
            case Kind::Function:
                return "Function";
            case Kind::Type:
                return "Type";
            case Kind::Parameter:
                return "Parameter";
            }
            return "Unknown";
        }

        std::string to_string() const
        {
            return name_ + " (" + kind_string() + "): " + type_->to_string();
        }
    };

    // === IMMUTABLE SCOPE ===

    class Scope
    {
    private:
        std::string name_;
        std::shared_ptr<Scope> parent_;
        std::unordered_map<std::string, Symbol> symbols_;

    public:
        Scope(std::string name, std::shared_ptr<Scope> parent = nullptr)
            : name_(std::move(name)), parent_(std::move(parent)) {}

        // Immutable operations - return new scope
        std::shared_ptr<Scope> add_symbol(Symbol symbol) const
        {
            auto new_scope = std::make_shared<Scope>(*this);
            new_scope->symbols_[symbol.name()] = std::move(symbol);
            return new_scope;
        }

        std::shared_ptr<Scope> add_symbols(std::vector<Symbol> symbols) const
        {
            auto new_scope = std::make_shared<Scope>(*this);
            for (auto &symbol : symbols)
            {
                new_scope->symbols_[symbol.name()] = std::move(symbol);
            }
            return new_scope;
        }

        // Lookups
        std::optional<Symbol> find_symbol(const std::string &name) const
        {
            // Local lookup
            auto it = symbols_.find(name);
            if (it != symbols_.end())
            {
                return it->second;
            }

            // Parent chain lookup
            if (parent_)
            {
                return parent_->find_symbol(name);
            }

            return std::nullopt;
        }

        // Type-safe member function resolution
        std::optional<Symbol> find_member_function(
            const std::string &type_name,
            const std::string &method_name) const
        {

            // Look for Type::method pattern
            std::string qualified_name = type_name + "::" + method_name;
            return find_symbol(qualified_name);
        }

        // Get all symbols in this scope (not including parent)
        std::vector<Symbol> get_local_symbols() const
        {
            std::vector<Symbol> result;
            for (const auto &[name, symbol] : symbols_)
            {
                result.push_back(symbol);
            }
            return result;
        }

        // Getters
        const std::string &name() const { return name_; }
        std::shared_ptr<Scope> parent() const { return parent_; }
        size_t symbol_count() const { return symbols_.size(); }

        std::string to_string() const
        {
            std::string result = "Scope '" + name_ + "' (" + std::to_string(symbols_.size()) + " symbols)";
            if (parent_)
            {
                result += " [parent: " + parent_->name() + "]";
            }
            return result;
        }
    };

    // === SYMBOL REGISTRY ===

    class SymbolRegistry
    {
    private:
        std::shared_ptr<Scope> global_scope_;

    public:
        SymbolRegistry() : global_scope_(std::make_shared<Scope>("global")) {}

        explicit SymbolRegistry(std::shared_ptr<Scope> global_scope)
            : global_scope_(std::move(global_scope)) {}

        // Immutable operations - return new registry
        SymbolRegistry add_type(const std::string &name,
                                std::shared_ptr<Type> type) const
        {
            Symbol type_symbol(name, Symbol::Kind::Type, type, SourceLocation{});
            auto new_global = global_scope_->add_symbol(std::move(type_symbol));

            return SymbolRegistry(new_global);
        }

        SymbolRegistry add_function(const std::string &name,
                                    std::shared_ptr<FunctionType> func_type,
                                    SourceLocation location = {}) const
        {
            Symbol func_symbol(name, Symbol::Kind::Function, func_type, location);
            auto new_global = global_scope_->add_symbol(std::move(func_symbol));

            return SymbolRegistry(new_global);
        }

        SymbolRegistry add_variable(const std::string &name,
                                    std::shared_ptr<Type> type,
                                    SourceLocation location = {}) const
        {
            Symbol var_symbol(name, Symbol::Kind::Variable, type, location);
            auto new_global = global_scope_->add_symbol(std::move(var_symbol));

            return SymbolRegistry(new_global);
        }

        // Add struct type with all its methods
        SymbolRegistry add_struct_type(std::shared_ptr<StructType> struct_type) const
        {
            auto registry = add_type(struct_type->name(), struct_type);

            // Add all methods as qualified functions
            for (const auto &method : struct_type->methods())
            {
                std::string qualified_name = struct_type->name() + "::" + method.name;

                // Create function type with 'this' parameter
                std::vector<std::shared_ptr<Type>> params;
                params.push_back(TypeFactory::create_pointer(struct_type)); // 'this' parameter
                params.insert(params.end(), method.parameter_types.begin(), method.parameter_types.end());

                auto func_type = TypeFactory::create_function(method.return_type, params);
                registry = registry.add_function(qualified_name, func_type);
            }

            return registry;
        }

        // Lookups
        std::optional<Symbol> lookup(const std::string &name) const
        {
            return global_scope_->find_symbol(name);
        }

        std::optional<Symbol> lookup_member_function(const std::string &type_name,
                                                     const std::string &method_name) const
        {
            return global_scope_->find_member_function(type_name, method_name);
        }

        // Get global scope for context creation
        std::shared_ptr<Scope> global_scope() const { return global_scope_; }

        std::string to_string() const
        {
            return global_scope_->to_string();
        }
    };

} // namespace Myre