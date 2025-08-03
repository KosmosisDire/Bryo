#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cassert>

namespace Myre
{

    // Forward declarations
    class PrimitiveType;
    class StructType;
    class FunctionType;
    class PointerType;
    class ArrayType;

    // === TYPE RTTI SYSTEM (like AST) ===

    enum class TypeKind
    {
        Primitive,
        Struct,
        Function,
        Pointer,
        Array
    };

    // Base type class - simple and lightweight
    class Type
    {
    protected:
        TypeKind kind_;

    public:
        explicit Type(TypeKind kind) : kind_(kind) {}
        virtual ~Type() = default;

        TypeKind kind() const { return kind_; }

        // RTTI helper methods (like AST system)
        template <typename T>
        bool is() const;

        template <typename T>
        T *as()
        {
            return is<T>() ? static_cast<T *>(this) : nullptr;
        }

        template <typename T>
        const T *as() const
        {
            return is<T>() ? static_cast<const T *>(this) : nullptr;
        }

        // Virtual methods
        virtual std::string to_string() const = 0;
        virtual bool equals(const Type &other) const = 0;
        virtual size_t hash() const = 0;
    };

    // === PRIMITIVE TYPES ===

    class PrimitiveType : public Type
    {
    private:
        std::string name_;
        size_t size_; // Size in bytes

    public:
        PrimitiveType(const std::string &name, size_t size)
            : Type(TypeKind::Primitive), name_(name), size_(size) {}

        const std::string &name() const { return name_; }
        size_t size() const { return size_; }

        std::string to_string() const override { return name_; }

        bool equals(const Type &other) const override
        {
            if (other.kind() != TypeKind::Primitive)
                return false;
            return name_ == static_cast<const PrimitiveType &>(other).name_;
        }

        size_t hash() const override
        {
            return std::hash<std::string>{}(name_);
        }
    };

    // === STRUCT TYPES ===

    struct Field
    {
        std::string name;
        Type *type; // Non-owning pointer
        size_t offset;

        Field(const std::string &n, Type *t, size_t o = 0)
            : name(n), type(t), offset(o) {}
    };

    struct Method
    {
        std::string name;
        FunctionType *type; // Non-owning pointer

        Method(const std::string &n, FunctionType *t)
            : name(n), type(t) {}
    };

    class StructType : public Type
    {
    private:
        std::string name_;
        std::vector<Field> fields_;
        std::vector<Method> methods_;
        size_t size_;
        size_t alignment_;

    public:
        StructType(const std::string &name)
            : Type(TypeKind::Struct), name_(name), size_(0), alignment_(1) {}

        // Mutable operations (during construction)
        void add_field(const std::string &name, Type *type);
        void add_method(const std::string &name, FunctionType *type);
        void finalize_layout(); // Calculate offsets and size

        // Getters
        const std::string &name() const { return name_; }
        const std::vector<Field> &fields() const { return fields_; }
        const std::vector<Method> &methods() const { return methods_; }
        size_t size() const { return size_; }
        size_t alignment() const { return alignment_; }

        // Lookups
        Field *find_field(const std::string &name);
        Method *find_method(const std::string &name);

        std::string to_string() const override { return "struct " + name_; }

        bool equals(const Type &other) const override
        {
            if (other.kind() != TypeKind::Struct)
                return false;
            // Struct types are compared by name (nominal typing)
            return name_ == static_cast<const StructType &>(other).name_;
        }

        size_t hash() const override
        {
            return std::hash<std::string>{}(name_);
        }
    };

    // === FUNCTION TYPES ===

    class FunctionType : public Type
    {
    private:
        Type *return_type_;                   // Non-owning
        std::vector<Type *> parameter_types_; // Non-owning
        bool is_varargs_;

    public:
        FunctionType(Type *return_type, std::vector<Type *> params, bool varargs = false)
            : Type(TypeKind::Function), return_type_(return_type), parameter_types_(std::move(params)), is_varargs_(varargs) {}

        Type *return_type() const { return return_type_; }
        const std::vector<Type *> &parameter_types() const { return parameter_types_; }
        bool is_varargs() const { return is_varargs_; }

        std::string to_string() const override;
        bool equals(const Type &other) const override;
        size_t hash() const override;
    };

    // === POINTER TYPES ===

    class PointerType : public Type
    {
    private:
        Type *pointee_type_; // Non-owning

    public:
        explicit PointerType(Type *pointee)
            : Type(TypeKind::Pointer), pointee_type_(pointee) {}

        Type *pointee_type() const { return pointee_type_; }

        std::string to_string() const override
        {
            return pointee_type_->to_string() + "*";
        }

        bool equals(const Type &other) const override
        {
            if (other.kind() != TypeKind::Pointer)
                return false;
            return pointee_type_->equals(*static_cast<const PointerType &>(other).pointee_type_);
        }

        size_t hash() const override
        {
            return pointee_type_->hash() ^ 0x12345678; // Mix with constant
        }
    };

    // === ARRAY TYPES ===

    class ArrayType : public Type
    {
    private:
        Type *element_type_; // Non-owning
        size_t size_;        // 0 for dynamic arrays

    public:
        ArrayType(Type *element, size_t size)
            : Type(TypeKind::Array), element_type_(element), size_(size) {}

        Type *element_type() const { return element_type_; }
        size_t size() const { return size_; }
        bool is_dynamic() const { return size_ == 0; }

        std::string to_string() const override
        {
            if (is_dynamic())
            {
                return element_type_->to_string() + "[]";
            }
            return element_type_->to_string() + "[" + std::to_string(size_) + "]";
        }

        bool equals(const Type &other) const override
        {
            if (other.kind() != TypeKind::Array)
                return false;
            const auto &other_array = static_cast<const ArrayType &>(other);
            return size_ == other_array.size_ &&
                   element_type_->equals(*other_array.element_type_);
        }

        size_t hash() const override
        {
            return element_type_->hash() ^ std::hash<size_t>{}(size_);
        }
    };

    // === RTTI TEMPLATE SPECIALIZATIONS ===

    template <>
    inline bool Type::is<PrimitiveType>() const { return kind_ == TypeKind::Primitive; }
    template <>
    inline bool Type::is<StructType>() const { return kind_ == TypeKind::Struct; }
    template <>
    inline bool Type::is<FunctionType>() const { return kind_ == TypeKind::Function; }
    template <>
    inline bool Type::is<PointerType>() const { return kind_ == TypeKind::Pointer; }
    template <>
    inline bool Type::is<ArrayType>() const { return kind_ == TypeKind::Array; }

} // namespace Myre