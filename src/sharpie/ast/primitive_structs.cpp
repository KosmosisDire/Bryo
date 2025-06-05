#include "sharpie/ast/primitive_structs.hpp"
#include "sharpie/ast/ast_enums.hpp"
#include <memory>

namespace Mycelium::Scripting::Lang
{
    void PrimitiveStructRegistry::initialize_builtin_primitives()
    {
        // Initialize Int32 (int)
        auto int32_info = std::make_unique<PrimitiveStructInfo>(
            PrimitiveStructKind::Int32, "System.Int32", "int", "i32");
        int32_info->struct_declaration = create_int32_struct();
        auto int32_ptr = int32_info.get();
        primitive_by_name["System.Int32"] = std::move(int32_info);
        primitive_by_simple_name["int"] = int32_ptr;
        primitive_by_kind[PrimitiveStructKind::Int32] = int32_ptr;

        // Initialize Boolean (bool)
        auto bool_info = std::make_unique<PrimitiveStructInfo>(
            PrimitiveStructKind::Boolean, "System.Boolean", "bool", "i1");
        bool_info->struct_declaration = create_boolean_struct();
        auto bool_ptr = bool_info.get();
        primitive_by_name["System.Boolean"] = std::move(bool_info);
        primitive_by_simple_name["bool"] = bool_ptr;
        primitive_by_kind[PrimitiveStructKind::Boolean] = bool_ptr;

        // Initialize String
        auto string_info = std::make_unique<PrimitiveStructInfo>(
            PrimitiveStructKind::String, "System.String", "string", "ptr");
        string_info->struct_declaration = create_string_struct();
        auto string_ptr = string_info.get();
        primitive_by_name["System.String"] = std::move(string_info);
        primitive_by_simple_name["string"] = string_ptr;
        primitive_by_kind[PrimitiveStructKind::String] = string_ptr;

        // Initialize Float
        auto float_info = std::make_unique<PrimitiveStructInfo>(
            PrimitiveStructKind::Float, "System.Single", "float", "float");
        float_info->struct_declaration = create_float_struct();
        auto float_ptr = float_info.get();
        primitive_by_name["System.Single"] = std::move(float_info);
        primitive_by_simple_name["float"] = float_ptr;
        primitive_by_kind[PrimitiveStructKind::Float] = float_ptr;

        // Initialize Double
        auto double_info = std::make_unique<PrimitiveStructInfo>(
            PrimitiveStructKind::Double, "System.Double", "double", "double");
        double_info->struct_declaration = create_double_struct();
        auto double_ptr = double_info.get();
        primitive_by_name["System.Double"] = std::move(double_info);
        primitive_by_simple_name["double"] = double_ptr;
        primitive_by_kind[PrimitiveStructKind::Double] = double_ptr;

        // Initialize Char
        auto char_info = std::make_unique<PrimitiveStructInfo>(
            PrimitiveStructKind::Char, "System.Char", "char", "i8");
        char_info->struct_declaration = create_char_struct();
        auto char_ptr = char_info.get();
        primitive_by_name["System.Char"] = std::move(char_info);
        primitive_by_simple_name["char"] = char_ptr;
        primitive_by_kind[PrimitiveStructKind::Char] = char_ptr;

        // Initialize Int64 (long)
        auto int64_info = std::make_unique<PrimitiveStructInfo>(
            PrimitiveStructKind::Int64, "System.Int64", "long", "i64");
        int64_info->struct_declaration = create_int64_struct();
        auto int64_ptr = int64_info.get();
        primitive_by_name["System.Int64"] = std::move(int64_info);
        primitive_by_simple_name["long"] = int64_ptr;
        primitive_by_kind[PrimitiveStructKind::Int64] = int64_ptr;
    }

    PrimitiveStructInfo* PrimitiveStructRegistry::get_by_name(const std::string& name)
    {
        auto it = primitive_by_name.find(name);
        return it != primitive_by_name.end() ? it->second.get() : nullptr;
    }

    PrimitiveStructInfo* PrimitiveStructRegistry::get_by_simple_name(const std::string& simple_name)
    {
        auto it = primitive_by_simple_name.find(simple_name);
        return it != primitive_by_simple_name.end() ? it->second : nullptr;
    }

    PrimitiveStructInfo* PrimitiveStructRegistry::get_by_kind(PrimitiveStructKind kind)
    {
        auto it = primitive_by_kind.find(kind);
        return it != primitive_by_kind.end() ? it->second : nullptr;
    }

    bool PrimitiveStructRegistry::is_primitive_struct(const std::string& type_name)
    {
        return primitive_by_name.find(type_name) != primitive_by_name.end();
    }

    bool PrimitiveStructRegistry::is_primitive_simple_name(const std::string& simple_name)
    {
        return primitive_by_simple_name.find(simple_name) != primitive_by_simple_name.end();
    }

    std::vector<PrimitiveStructInfo*> PrimitiveStructRegistry::get_all_primitives()
    {
        std::vector<PrimitiveStructInfo*> result;
        for (auto& pair : primitive_by_name)
        {
            result.push_back(pair.second.get());
        }
        return result;
    }

    // Helper function to create TypeNameNode
    std::shared_ptr<TypeNameNode> create_type_name(const std::string& type_name)
    {
        auto type_node = std::make_shared<TypeNameNode>();
        auto ident = std::make_shared<IdentifierNode>();
        ident->name = type_name;
        type_node->name_segment = ident;
        return type_node;
    }

    // Helper function to create IdentifierNode  
    std::shared_ptr<IdentifierNode> create_identifier(const std::string& name)
    {
        auto ident = std::make_shared<IdentifierNode>();
        ident->name = name;
        return ident;
    }

    // Helper function to create TokenNode
    std::shared_ptr<TokenNode> create_token(const std::string& text)
    {
        auto token = std::make_shared<TokenNode>();
        token->text = text;
        return token;
    }

    std::shared_ptr<MethodDeclarationNode> create_primitive_method(
        const std::string& method_name,
        std::shared_ptr<TypeNameNode> return_type,
        const std::vector<std::shared_ptr<ParameterDeclarationNode>>& parameters,
        bool is_static)
    {
        auto method = std::make_shared<MethodDeclarationNode>();
        method->name = create_identifier(method_name);
        method->type = return_type;
        method->parameters = parameters;
        
        if (is_static)
        {
            method->modifiers.push_back({ModifierKind::Static, create_token("static")});
        }
        method->modifiers.push_back({ModifierKind::Public, create_token("public")});

        method->openParenToken = create_token("(");
        method->closeParenToken = create_token(")");
        method->semicolonToken = create_token(";");

        return method;
    }

    std::shared_ptr<StructDeclarationNode> create_int32_struct()
    {
        auto struct_decl = std::make_shared<StructDeclarationNode>();
        struct_decl->name = create_identifier("Int32");
        struct_decl->typeKeywordToken = create_token("struct");
        struct_decl->openBraceToken = create_token("{");
        struct_decl->closeBraceToken = create_token("}");

        // Add ToString method
        auto to_string_method = create_primitive_method(
            "ToString", create_type_name("string"), {}, false);
        struct_decl->members.push_back(to_string_method);

        // Add Parse static method  
        auto parse_param = std::make_shared<ParameterDeclarationNode>();
        parse_param->name = create_identifier("s");
        parse_param->type = create_type_name("string");
        
        auto parse_method = create_primitive_method(
            "Parse", create_type_name("int"), {parse_param}, true);
        struct_decl->members.push_back(parse_method);

        // Add TryParse static method
        auto try_parse_s_param = std::make_shared<ParameterDeclarationNode>();
        try_parse_s_param->name = create_identifier("s");
        try_parse_s_param->type = create_type_name("string");

        auto try_parse_result_param = std::make_shared<ParameterDeclarationNode>();
        try_parse_result_param->name = create_identifier("result");
        try_parse_result_param->type = create_type_name("int");

        auto try_parse_method = create_primitive_method(
            "TryParse", create_type_name("bool"), {try_parse_s_param, try_parse_result_param}, true);
        struct_decl->members.push_back(try_parse_method);

        return struct_decl;
    }

    std::shared_ptr<StructDeclarationNode> create_boolean_struct()
    {
        auto struct_decl = std::make_shared<StructDeclarationNode>();
        struct_decl->name = create_identifier("Boolean");
        struct_decl->typeKeywordToken = create_token("struct");
        struct_decl->openBraceToken = create_token("{");
        struct_decl->closeBraceToken = create_token("}");

        // Add ToString method
        auto to_string_method = create_primitive_method(
            "ToString", create_type_name("string"), {}, false);
        struct_decl->members.push_back(to_string_method);

        // Add Parse static method
        auto parse_param = std::make_shared<ParameterDeclarationNode>();
        parse_param->name = create_identifier("s");
        parse_param->type = create_type_name("string");
        
        auto parse_method = create_primitive_method(
            "Parse", create_type_name("bool"), {parse_param}, true);
        struct_decl->members.push_back(parse_method);

        return struct_decl;
    }

    std::shared_ptr<StructDeclarationNode> create_string_struct()
    {
        auto struct_decl = std::make_shared<StructDeclarationNode>();
        struct_decl->name = create_identifier("String");
        struct_decl->typeKeywordToken = create_token("struct");
        struct_decl->openBraceToken = create_token("{");
        struct_decl->closeBraceToken = create_token("}");

        // Add Length property (getter method for now)
        auto length_method = create_primitive_method(
            "get_Length", create_type_name("int"), {}, false);
        struct_decl->members.push_back(length_method);

        // Add Substring method
        auto start_param = std::make_shared<ParameterDeclarationNode>();
        start_param->name = create_identifier("startIndex");
        start_param->type = create_type_name("int");

        auto substring_method = create_primitive_method(
            "Substring", create_type_name("string"), {start_param}, false);
        struct_decl->members.push_back(substring_method);

        // Add static Empty property
        auto empty_method = create_primitive_method(
            "get_Empty", create_type_name("string"), {}, true);
        struct_decl->members.push_back(empty_method);

        return struct_decl;
    }

    std::shared_ptr<StructDeclarationNode> create_float_struct()
    {
        auto struct_decl = std::make_shared<StructDeclarationNode>();
        struct_decl->name = create_identifier("Single");
        struct_decl->typeKeywordToken = create_token("struct");
        struct_decl->openBraceToken = create_token("{");
        struct_decl->closeBraceToken = create_token("}");

        // Add ToString method
        auto to_string_method = create_primitive_method(
            "ToString", create_type_name("string"), {}, false);
        struct_decl->members.push_back(to_string_method);

        // Add Parse static method
        auto parse_param = std::make_shared<ParameterDeclarationNode>();
        parse_param->name = create_identifier("s");
        parse_param->type = create_type_name("string");
        
        auto parse_method = create_primitive_method(
            "Parse", create_type_name("float"), {parse_param}, true);
        struct_decl->members.push_back(parse_method);

        return struct_decl;
    }

    std::shared_ptr<StructDeclarationNode> create_double_struct()
    {
        auto struct_decl = std::make_shared<StructDeclarationNode>();
        struct_decl->name = create_identifier("Double");
        struct_decl->typeKeywordToken = create_token("struct");
        struct_decl->openBraceToken = create_token("{");
        struct_decl->closeBraceToken = create_token("}");

        // Add ToString method
        auto to_string_method = create_primitive_method(
            "ToString", create_type_name("string"), {}, false);
        struct_decl->members.push_back(to_string_method);

        // Add Parse static method
        auto parse_param = std::make_shared<ParameterDeclarationNode>();
        parse_param->name = create_identifier("s");
        parse_param->type = create_type_name("string");
        
        auto parse_method = create_primitive_method(
            "Parse", create_type_name("double"), {parse_param}, true);
        struct_decl->members.push_back(parse_method);

        return struct_decl;
    }

    std::shared_ptr<StructDeclarationNode> create_char_struct()
    {
        auto struct_decl = std::make_shared<StructDeclarationNode>();
        struct_decl->name = create_identifier("Char");
        struct_decl->typeKeywordToken = create_token("struct");
        struct_decl->openBraceToken = create_token("{");
        struct_decl->closeBraceToken = create_token("}");

        // Add ToString method
        auto to_string_method = create_primitive_method(
            "ToString", create_type_name("string"), {}, false);
        struct_decl->members.push_back(to_string_method);

        return struct_decl;
    }

    std::shared_ptr<StructDeclarationNode> create_int64_struct()
    {
        auto struct_decl = std::make_shared<StructDeclarationNode>();
        struct_decl->name = create_identifier("Int64");
        struct_decl->typeKeywordToken = create_token("struct");
        struct_decl->openBraceToken = create_token("{");
        struct_decl->closeBraceToken = create_token("}");

        // Add ToString method
        auto to_string_method = create_primitive_method(
            "ToString", create_type_name("string"), {}, false);
        struct_decl->members.push_back(to_string_method);

        // Add Parse static method
        auto parse_param = std::make_shared<ParameterDeclarationNode>();
        parse_param->name = create_identifier("s");
        parse_param->type = create_type_name("string");
        
        auto parse_method = create_primitive_method(
            "Parse", create_type_name("long"), {parse_param}, true);
        struct_decl->members.push_back(parse_method);

        return struct_decl;
    }
}
