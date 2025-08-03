#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>

namespace Myre
{

    // === IMMUTABLE TYPE SYSTEM ===

    class Type
    {
    public:
        enum class Kind
        {
            Primitive,
            Struct,
            Function,
            Pointer,
            Array
        };

        virtual ~Type() = default;
        virtual Kind kind() const = 0;
        virtual std::string name() const = 0;
        virtual bool equals(const Type &other) const = 0;
        virtual std::string to_string() const = 0;
    };

    class PrimitiveType : public Type
    {
    private:
        std::string name_;

    public:
        explicit PrimitiveType(std::string name) : name_(std::move(name)) {}

        Kind kind() const override { return Kind::Primitive; }
        std::string name() const override { return name_; }
        std::string to_string() const override { return name_; }

        bool equals(const Type &other) const override
        {
            if (other.kind() != Kind::Primitive)
                return false;
            return name_ == static_cast<const PrimitiveType &>(other).name_;
        }
    };

    struct FieldInfo
    {
        std::string name;
        std::shared_ptr<Type> type;
        int offset; // For struct layout

        FieldInfo(std::string n, std::shared_ptr<Type> t, int o = 0)
            : name(std::move(n)), type(std::move(t)), offset(o) {}
    };

    struct MethodInfo
    {
        std::string name;
        std::shared_ptr<Type> return_type;
        std::vector<std::shared_ptr<Type>> parameter_types;

        MethodInfo(std::string n, std::shared_ptr<Type> ret,
                   std::vector<std::shared_ptr<Type>> params = {})
            : name(std::move(n)), return_type(std::move(ret)), parameter_types(std::move(params)) {}
    };

    class StructType : public Type
    {
    private:
        std::string name_;
        std::vector<FieldInfo> fields_;
        std::vector<MethodInfo> methods_;

    public:
        StructType(std::string name,
                   std::vector<FieldInfo> fields,
                   std::vector<MethodInfo> methods)
            : name_(std::move(name)), fields_(std::move(fields)), methods_(std::move(methods)) {}

        Kind kind() const override { return Kind::Struct; }
        std::string name() const override { return name_; }
        std::string to_string() const override { return "struct " + name_; }

        bool equals(const Type &other) const override
        {
            if (other.kind() != Kind::Struct)
                return false;
            return name_ == static_cast<const StructType &>(other).name_;
        }

        // Immutable getters
        const std::vector<FieldInfo> &fields() const { return fields_; }
        const std::vector<MethodInfo> &methods() const { return methods_; }

        // Type-safe lookups
        std::optional<FieldInfo> find_field(const std::string &name) const
        {
            auto it = std::find_if(fields_.begin(), fields_.end(),
                                   [&name](const FieldInfo &f)
                                   { return f.name == name; });
            if (it != fields_.end())
            {
                return *it;
            }
            return std::nullopt;
        }

        std::optional<MethodInfo> find_method(const std::string &name) const
        {
            auto it = std::find_if(methods_.begin(), methods_.end(),
                                   [&name](const MethodInfo &m)
                                   { return m.name == name; });
            if (it != methods_.end())
            {
                return *it;
            }
            return std::nullopt;
        }
    };

    class FunctionType : public Type
    {
    private:
        std::shared_ptr<Type> return_type_;
        std::vector<std::shared_ptr<Type>> parameter_types_;

    public:
        FunctionType(std::shared_ptr<Type> return_type,
                     std::vector<std::shared_ptr<Type>> parameter_types)
            : return_type_(std::move(return_type)), parameter_types_(std::move(parameter_types)) {}

        Kind kind() const override { return Kind::Function; }
        std::string name() const override { return to_string(); }
        std::string to_string() const override
        {
            std::string result = "(";
            for (size_t i = 0; i < parameter_types_.size(); ++i)
            {
                if (i > 0)
                    result += ", ";
                result += parameter_types_[i]->to_string();
            }
            result += ") -> " + return_type_->to_string();
            return result;
        }

        bool equals(const Type &other) const override
        {
            if (other.kind() != Kind::Function)
                return false;
            const auto &other_func = static_cast<const FunctionType &>(other);

            if (!return_type_->equals(*other_func.return_type_))
                return false;
            if (parameter_types_.size() != other_func.parameter_types_.size())
                return false;

            for (size_t i = 0; i < parameter_types_.size(); ++i)
            {
                if (!parameter_types_[i]->equals(*other_func.parameter_types_[i]))
                {
                    return false;
                }
            }
            return true;
        }

        const Type &return_type() const { return *return_type_; }
        const std::vector<std::shared_ptr<Type>> &parameter_types() const
        {
            return parameter_types_;
        }
    };

    class PointerType : public Type
    {
    private:
        std::shared_ptr<Type> pointee_type_;

    public:
        explicit PointerType(std::shared_ptr<Type> pointee_type)
            : pointee_type_(std::move(pointee_type)) {}

        Kind kind() const override { return Kind::Pointer; }
        std::string name() const override { return to_string(); }
        std::string to_string() const override
        {
            return pointee_type_->to_string() + "*";
        }

        bool equals(const Type &other) const override
        {
            if (other.kind() != Kind::Pointer)
                return false;
            return pointee_type_->equals(*static_cast<const PointerType &>(other).pointee_type_);
        }

        const Type &pointee_type() const { return *pointee_type_; }
    };

    // === TYPE FACTORY ===

    class TypeFactory
    {
    private:
        static std::unordered_map<std::string, std::shared_ptr<PrimitiveType>> primitive_cache_;

    public:
        static std::shared_ptr<PrimitiveType> get_primitive(const std::string &name)
        {
            auto it = primitive_cache_.find(name);
            if (it != primitive_cache_.end())
            {
                return it->second;
            }

            auto type = std::make_shared<PrimitiveType>(name);
            primitive_cache_[name] = type;
            return type;
        }

        static std::shared_ptr<StructType> create_struct(
            const std::string &name,
            std::vector<FieldInfo> fields,
            std::vector<MethodInfo> methods)
        {
            return std::make_shared<StructType>(name, std::move(fields), std::move(methods));
        }

        static std::shared_ptr<FunctionType> create_function(
            std::shared_ptr<Type> return_type,
            std::vector<std::shared_ptr<Type>> parameter_types)
        {
            return std::make_shared<FunctionType>(std::move(return_type), std::move(parameter_types));
        }

        static std::shared_ptr<PointerType> create_pointer(std::shared_ptr<Type> pointee_type)
        {
            return std::make_shared<PointerType>(std::move(pointee_type));
        }

        // Common types
        static std::shared_ptr<PrimitiveType> i32() { return get_primitive("i32"); }
        static std::shared_ptr<PrimitiveType> bool_type() { return get_primitive("bool"); }
        static std::shared_ptr<PrimitiveType> void_type() { return get_primitive("void"); }
    };

} // namespace Myre