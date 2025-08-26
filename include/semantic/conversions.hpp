#pragma once

#include "type.hpp"
#include <string>
#include <unordered_map>

// inspired by https://github.com/dotnet/roslyn/blob/main/src/Compilers/CSharp/Portable/Binder/Semantics/Conversions/ConversionEasyOut.cs
// from the roslyn project, licensed under the MIT license.
namespace Bryo
{
    /**
     * @enum ConversionKind
     * @brief Represents the kind of type conversion between two types
     */
    enum class ConversionKind : uint8_t
    {
        NoConversion = 0, // No conversion possible
        Identity,         // Same type, no conversion needed
        ImplicitNumeric,  // Implicit numeric widening (safe)
        ExplicitNumeric,  // Explicit numeric conversion (may lose precision)
    };

    /**
     * @class Conversions
     * @brief Manages type conversion rules between primitive types
     */
    class Conversions
    {
    private:
        // Short names for readability in the matrix
        static constexpr ConversionKind NOC = ConversionKind::NoConversion;
        static constexpr ConversionKind IDN = ConversionKind::Identity;
        static constexpr ConversionKind IMP = ConversionKind::ImplicitNumeric;
        static constexpr ConversionKind EXP = ConversionKind::ExplicitNumeric;

        // Conversion matrix using PrimitiveType::Kind as indices
        // Rows = source type, Columns = target type
        static constexpr ConversionKind conversionMatrix[14][14] = {
            // Converting FROM (row) TO (column):
            //          i8   u8   i16  u16  i32  u32  i64  u64  f32  f64  bool char void
            /*  i8 */ {IDN, EXP, IMP, EXP, IMP, EXP, IMP, EXP, IMP, IMP, EXP, EXP, NOC},
            /*  u8 */ {EXP, IDN, IMP, IMP, IMP, IMP, IMP, IMP, IMP, IMP, EXP, EXP, NOC},
            /* i16 */ {EXP, EXP, IDN, EXP, IMP, EXP, IMP, EXP, IMP, IMP, EXP, EXP, NOC},
            /* u16 */ {EXP, EXP, EXP, IDN, IMP, IMP, IMP, IMP, IMP, IMP, EXP, EXP, NOC},
            /* i32 */ {EXP, EXP, EXP, EXP, IDN, EXP, IMP, EXP, IMP, IMP, EXP, EXP, NOC},
            /* u32 */ {EXP, EXP, EXP, EXP, EXP, IDN, IMP, IMP, IMP, IMP, EXP, EXP, NOC},
            /* i64 */ {EXP, EXP, EXP, EXP, EXP, EXP, IDN, EXP, EXP, IMP, EXP, EXP, NOC},
            /* u64 */ {EXP, EXP, EXP, EXP, EXP, EXP, EXP, IDN, EXP, IMP, EXP, EXP, NOC},
            /* f32 */ {EXP, EXP, EXP, EXP, EXP, EXP, EXP, EXP, IDN, IMP, EXP, EXP, NOC},
            /* f64 */ {EXP, EXP, EXP, EXP, EXP, EXP, EXP, EXP, EXP, IDN, EXP, EXP, NOC},
            /*bool */ {EXP, EXP, EXP, EXP, EXP, EXP, EXP, EXP, EXP, EXP, IDN, EXP, NOC},
            /*char */ {EXP, EXP, IMP, IMP, IMP, IMP, IMP, IMP, IMP, IMP, EXP, IDN, NOC},
            /*void */ {NOC, NOC, NOC, NOC, NOC, NOC, NOC, NOC, NOC, NOC, NOC, NOC, IDN},
        };

    public:
        /**
         * Get primitive type kind from string name
         */
        static PrimitiveType::Kind GetPrimitiveKind(const std::string &typeName)
        {
            static const std::unordered_map<std::string, PrimitiveType::Kind> typeMap = {
                {"i8", PrimitiveType::I8},
                {"u8", PrimitiveType::U8},
                {"i16", PrimitiveType::I16},
                {"u16", PrimitiveType::U16},
                {"i32", PrimitiveType::I32},
                {"u32", PrimitiveType::U32},
                {"i64", PrimitiveType::I64},
                {"u64", PrimitiveType::U64},
                {"f32", PrimitiveType::F32},
                {"f64", PrimitiveType::F64},
                {"bool", PrimitiveType::Bool},
                {"char", PrimitiveType::Char},
                {"void", PrimitiveType::Void}};

            auto it = typeMap.find(typeName);
            if (it != typeMap.end())
                return it->second;

            // Return void as default for unknown types
            return PrimitiveType::Void;
        }

        /**
         * Classify the conversion between two primitive types
         */
        static ConversionKind ClassifyConversion(PrimitiveType::Kind source, PrimitiveType::Kind target)
        {
            // Direct indexing using the enum values
            return conversionMatrix[source][target];
        }

        /**
         * Classify the conversion between two types (TypePtr version)
         */
        static ConversionKind ClassifyConversion(TypePtr sourceType, TypePtr targetType)
        {
            // Handle array conversions
            auto sourceArray = std::get_if<ArrayType>(&sourceType->value);
            auto targetArray = std::get_if<ArrayType>(&targetType->value);

            if (sourceArray && targetArray)
            {
                // Array to array conversion
                // Check if element types match
                if (sourceArray->elementType == targetArray->elementType ||
                    sourceArray->elementType->get_name() == targetArray->elementType->get_name())
                {
                    // Sized array to unsized array is allowed (char[12] -> char[])
                    // Unsized to unsized is allowed (char[] -> char[])
                    // Sized to same-sized is allowed (char[12] -> char[12])
                    // But sized to different-sized is not (char[12] -> char[10])
                    if (targetArray->fixedSize == -1 ||              // Target is unsized array
                        sourceArray->fixedSize == -1 ||              // Source is unsized array
                        sourceArray->fixedSize == targetArray->fixedSize) // Same size
                    {
                        return ConversionKind::Identity; // Treat as identity for parameter passing
                    }
                }
                return ConversionKind::NoConversion;
            }

            // Handle array to pointer decay
            auto targetPointer = std::get_if<PointerType>(&targetType->value);
            if (sourceArray && targetPointer)
            {
                // Check if array element type matches pointer target type
                if (sourceArray->elementType == targetPointer->pointeeType ||
                    sourceArray->elementType->get_name() == targetPointer->pointeeType->get_name())
                {
                    return ConversionKind::Identity; // Array-to-pointer decay is implicit
                }
            }

            // Handle pointer types - identity only if same pointer type
            auto sourcePointer = std::get_if<PointerType>(&sourceType->value);
            if (sourcePointer && targetPointer)
            {
                if (sourcePointer->pointeeType == targetPointer->pointeeType ||
                    sourcePointer->pointeeType->get_name() == targetPointer->pointeeType->get_name())
                {
                    return ConversionKind::Identity;
                }
                return ConversionKind::NoConversion;
            }

            // Handle primitive types
            auto sourcePrim = std::get_if<PrimitiveType>(&sourceType->value);
            auto targetPrim = std::get_if<PrimitiveType>(&targetType->value);

            if (sourcePrim && targetPrim)
            {
                return ClassifyConversion(sourcePrim->kind, targetPrim->kind);
            }

            // For all other types (TypeReference, GenericType, etc.), only identity conversions
            if (sourceType == targetType || sourceType->get_name() == targetType->get_name())
                return ConversionKind::Identity;

            return ConversionKind::NoConversion;
        }
        
        /**
         * Check if a conversion is implicit (can be done automatically)
         */
        static bool IsImplicitConversion(ConversionKind kind)
        {
            // TODO: Support implicit casting
            return kind == ConversionKind::Identity;
        }

        /**
         * Check if a conversion requires an explicit cast
         */
        static bool IsExplicitConversion(ConversionKind kind)
        {
            // TODO: Support implicit casting without forcing explicit
            return kind == ConversionKind::ExplicitNumeric || kind == ConversionKind::ImplicitNumeric;
        }

        /**
         * Check if any conversion is possible
         */
        static bool IsConversionPossible(ConversionKind kind)
        {
            return kind != ConversionKind::NoConversion;
        }

        /**
         * Get a human-readable description of the conversion
         */
        static std::string GetConversionDescription(ConversionKind kind)
        {
            switch (kind)
            {
            case ConversionKind::NoConversion:
                return "no conversion";
            case ConversionKind::Identity:
                return "identity";
            case ConversionKind::ImplicitNumeric:
                return "implicit numeric conversion";
            case ConversionKind::ExplicitNumeric:
                return "explicit numeric conversion";
            default:
                return "unknown conversion";
            }
        }
    };

} // namespace Bryo