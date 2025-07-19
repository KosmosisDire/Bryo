#include "ast/ast.hpp"
#include <vector>
#include <stdexcept>

namespace Mycelium::Scripting::Lang
{
    // --- RTTI System Implementation ---

    // Global registry of all type information, populated by the static initializers.
    static std::vector<AstTypeInfo*> g_all_type_infos;

    // The final, ordered list of type information, indexed by TypeId.
    // This is the definition for the 'extern' variable in ast_rtti.hpp
    std::vector<AstTypeInfo*> g_ordered_type_infos;

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
        g_ordered_type_infos.push_back(type_info);
        for (auto derived_type : type_info->derivedTypes)
        {
            order_types_recursive(derived_type);
        }
    }

    void AstTypeInfo::initialize()
    {
        // Prevent double initialization
        if (!g_ordered_type_infos.empty())
        {
            return;
        }

        // The base AstNode must be the root. Find it.
        AstTypeInfo* root = &AstNode::sTypeInfo;

        // Recursively walk the type hierarchy to create a flattened, ordered list.
        g_ordered_type_infos.reserve(g_all_type_infos.size());
        order_types_recursive(root);

        // Assign type IDs based on the flattened order.
        for (size_t i = 0; i < g_ordered_type_infos.size(); ++i)
        {
            g_ordered_type_infos[i]->typeId = static_cast<uint8_t>(i);
        }

        // Calculate the full derived count for each type.
        for (size_t i = 0; i < g_ordered_type_infos.size(); ++i)
        {
            AstTypeInfo* current_type = g_ordered_type_infos[i];
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
        #ifdef AST_HAS_PARENT_POINTER
        this->parent = nullptr;
        #endif
        this->typeId = id;
        this->tokenKind = TokenKind::None;
        this->sourceStart = 0;
        this->sourceLength = 0;
        this->triviaStart = 0;
    }

    void AstNode::accept(StructuralVisitor* visitor)
    {
        // Dispatch to the correct visit method using our RTTI system.
        g_ordered_type_infos[this->typeId]->acceptFunc(this, visitor);
    }

    std::string_view AstNode::to_string_view() const
    {
        // This requires access to the source file buffer, which is typically
        // managed by the parser or a context object.
        // For now, it returns an empty view.
        return {};
    }

    // --- AST_DECL_IMPL Definitions ---
    // This defines the static sTypeInfo member for every AST node type,
    // effectively registering them with the RTTI system upon program startup.

    AST_DECL_ROOT_IMPL(AstNode)
    AST_DECL_IMPL(TokenNode, AstNode)
    AST_DECL_IMPL(IdentifierNode, AstNode)

    AST_DECL_IMPL(ExpressionNode, AstNode)
    AST_DECL_IMPL(LiteralExpressionNode, ExpressionNode)
    AST_DECL_IMPL(IdentifierExpressionNode, ExpressionNode)
    AST_DECL_IMPL(ParenthesizedExpressionNode, ExpressionNode)
    AST_DECL_IMPL(UnaryExpressionNode, ExpressionNode)
    AST_DECL_IMPL(BinaryExpressionNode, ExpressionNode)
    AST_DECL_IMPL(AssignmentExpressionNode, ExpressionNode)
    AST_DECL_IMPL(CallExpressionNode, ExpressionNode)
    AST_DECL_IMPL(MemberAccessExpressionNode, ExpressionNode)
    AST_DECL_IMPL(NewExpressionNode, ExpressionNode)
    AST_DECL_IMPL(ThisExpressionNode, ExpressionNode)
    AST_DECL_IMPL(CastExpressionNode, ExpressionNode)
    AST_DECL_IMPL(IndexerExpressionNode, ExpressionNode)
    AST_DECL_IMPL(TypeOfExpressionNode, ExpressionNode)
    AST_DECL_IMPL(SizeOfExpressionNode, ExpressionNode)
    AST_DECL_IMPL(MatchExpressionNode, ExpressionNode)
    AST_DECL_IMPL(ConditionalExpressionNode, ExpressionNode)
    AST_DECL_IMPL(RangeExpressionNode, ExpressionNode)
    AST_DECL_IMPL(EnumMemberExpressionNode, ExpressionNode)
    AST_DECL_IMPL(FieldKeywordExpressionNode, ExpressionNode)
    AST_DECL_IMPL(ValueKeywordExpressionNode, ExpressionNode)

    AST_DECL_IMPL(StatementNode, AstNode)
    AST_DECL_IMPL(EmptyStatementNode, StatementNode)
    AST_DECL_IMPL(BlockStatementNode, StatementNode)
    AST_DECL_IMPL(ExpressionStatementNode, StatementNode)
    AST_DECL_IMPL(IfStatementNode, StatementNode)
    AST_DECL_IMPL(WhileStatementNode, StatementNode)
    AST_DECL_IMPL(ForStatementNode, StatementNode)
    AST_DECL_IMPL(ReturnStatementNode, StatementNode)
    AST_DECL_IMPL(BreakStatementNode, StatementNode)
    AST_DECL_IMPL(ContinueStatementNode, StatementNode)
    AST_DECL_IMPL(LocalVariableDeclarationNode, StatementNode)
    AST_DECL_IMPL(UsingDirectiveNode, StatementNode)

    AST_DECL_IMPL(DeclarationNode, StatementNode)
    AST_DECL_IMPL(ParameterNode, DeclarationNode)
    AST_DECL_IMPL(VariableDeclarationNode, DeclarationNode)
    AST_DECL_IMPL(MemberDeclarationNode, DeclarationNode)
    AST_DECL_IMPL(FieldDeclarationNode, MemberDeclarationNode)
    AST_DECL_IMPL(GenericParameterNode, DeclarationNode)
    AST_DECL_IMPL(FunctionDeclarationNode, MemberDeclarationNode)
    AST_DECL_IMPL(PropertyDeclarationNode, MemberDeclarationNode)
    AST_DECL_IMPL(ConstructorDeclarationNode, MemberDeclarationNode)
    AST_DECL_IMPL(EnumCaseNode, MemberDeclarationNode)
    AST_DECL_IMPL(TypeDeclarationNode, DeclarationNode)
    AST_DECL_IMPL(InterfaceDeclarationNode, DeclarationNode)
    AST_DECL_IMPL(EnumDeclarationNode, DeclarationNode)
    AST_DECL_IMPL(NamespaceDeclarationNode, DeclarationNode)
    
    // Match patterns
    AST_DECL_IMPL(MatchArmNode, AstNode)
    AST_DECL_IMPL(MatchPatternNode, AstNode)
    AST_DECL_IMPL(EnumPatternNode, MatchPatternNode)
    AST_DECL_IMPL(RangePatternNode, MatchPatternNode)
    AST_DECL_IMPL(ComparisonPatternNode, MatchPatternNode)
    AST_DECL_IMPL(WildcardPatternNode, MatchPatternNode)
    AST_DECL_IMPL(LiteralPatternNode, MatchPatternNode)
    
    // Property accessor
    AST_DECL_IMPL(PropertyAccessorNode, AstNode)

    AST_DECL_IMPL(TypeNameNode, AstNode)
    AST_DECL_IMPL(QualifiedTypeNameNode, TypeNameNode)
    AST_DECL_IMPL(PointerTypeNameNode, TypeNameNode)
    AST_DECL_IMPL(ArrayTypeNameNode, TypeNameNode)
    AST_DECL_IMPL(GenericTypeNameNode, TypeNameNode)

    AST_DECL_IMPL(CompilationUnitNode, AstNode)

    // --- class_accept Method Implementations ---
    // These methods are called by the RTTI system to dispatch to the correct visit method.

    void AstNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<AstNode*>(node)); }
    void TokenNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<TokenNode*>(node)); }
    void IdentifierNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<IdentifierNode*>(node)); }

    // Expressions
    void ExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ExpressionNode*>(node)); }
    void LiteralExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<LiteralExpressionNode*>(node)); }
    void IdentifierExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<IdentifierExpressionNode*>(node)); }
    void ParenthesizedExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ParenthesizedExpressionNode*>(node)); }
    void UnaryExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<UnaryExpressionNode*>(node)); }
    void BinaryExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<BinaryExpressionNode*>(node)); }
    void AssignmentExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<AssignmentExpressionNode*>(node)); }
    void CallExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<CallExpressionNode*>(node)); }
    void MemberAccessExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<MemberAccessExpressionNode*>(node)); }
    void NewExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<NewExpressionNode*>(node)); }
    void ThisExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ThisExpressionNode*>(node)); }
    void CastExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<CastExpressionNode*>(node)); }
    void IndexerExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<IndexerExpressionNode*>(node)); }
    void TypeOfExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<TypeOfExpressionNode*>(node)); }
    void SizeOfExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<SizeOfExpressionNode*>(node)); }
    void MatchExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<MatchExpressionNode*>(node)); }
    void ConditionalExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ConditionalExpressionNode*>(node)); }
    void RangeExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<RangeExpressionNode*>(node)); }
    void EnumMemberExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<EnumMemberExpressionNode*>(node)); }
    void FieldKeywordExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<FieldKeywordExpressionNode*>(node)); }
    void ValueKeywordExpressionNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ValueKeywordExpressionNode*>(node)); }

    // Statements
    void StatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<StatementNode*>(node)); }
    void EmptyStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<EmptyStatementNode*>(node)); }
    void BlockStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<BlockStatementNode*>(node)); }
    void ExpressionStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ExpressionStatementNode*>(node)); }
    void IfStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<IfStatementNode*>(node)); }
    void WhileStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<WhileStatementNode*>(node)); }
    void ForStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ForStatementNode*>(node)); }
    void ReturnStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ReturnStatementNode*>(node)); }
    void BreakStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<BreakStatementNode*>(node)); }
    void ContinueStatementNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ContinueStatementNode*>(node)); }
    void LocalVariableDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<LocalVariableDeclarationNode*>(node)); }
    void UsingDirectiveNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<UsingDirectiveNode*>(node)); }

    // Declarations
    void DeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<DeclarationNode*>(node)); }
    void NamespaceDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<NamespaceDeclarationNode*>(node)); }
    void TypeDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<TypeDeclarationNode*>(node)); }
    void InterfaceDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<InterfaceDeclarationNode*>(node)); }
    void EnumDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<EnumDeclarationNode*>(node)); }
    void MemberDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<MemberDeclarationNode*>(node)); }
    void FieldDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<FieldDeclarationNode*>(node)); }
    void FunctionDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<FunctionDeclarationNode*>(node)); }
    void PropertyDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<PropertyDeclarationNode*>(node)); }
    void ConstructorDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ConstructorDeclarationNode*>(node)); }
    void EnumCaseNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<EnumCaseNode*>(node)); }
    void ParameterNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ParameterNode*>(node)); }
    void VariableDeclarationNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<VariableDeclarationNode*>(node)); }
    void GenericParameterNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<GenericParameterNode*>(node)); }
    
    // Match patterns
    void MatchArmNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<MatchArmNode*>(node)); }
    void MatchPatternNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<MatchPatternNode*>(node)); }
    void EnumPatternNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<EnumPatternNode*>(node)); }
    void RangePatternNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<RangePatternNode*>(node)); }
    void ComparisonPatternNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ComparisonPatternNode*>(node)); }
    void WildcardPatternNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<WildcardPatternNode*>(node)); }
    void LiteralPatternNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<LiteralPatternNode*>(node)); }
    
    // Property accessor
    void PropertyAccessorNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<PropertyAccessorNode*>(node)); }

    // Types
    void TypeNameNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<TypeNameNode*>(node)); }
    void QualifiedTypeNameNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<QualifiedTypeNameNode*>(node)); }
    void PointerTypeNameNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<PointerTypeNameNode*>(node)); }
    void ArrayTypeNameNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<ArrayTypeNameNode*>(node)); }
    void GenericTypeNameNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<GenericTypeNameNode*>(node)); }

    // Root
    void CompilationUnitNode::class_accept(AstNode* node, StructuralVisitor* visitor) { visitor->visit(static_cast<CompilationUnitNode*>(node)); }

    // --- StructuralVisitor Method Implementations ---
    // Default behavior is to visit the node's base type, creating a chain
    // up the inheritance hierarchy.

    #define DEF_VISITOR_IMPL(NodeType, BaseType) \
        void StructuralVisitor::visit(NodeType* node) { visit(static_cast<BaseType*>(node)); }

    void StructuralVisitor::visit(AstNode* node) { /* Base case: do nothing */ }

    DEF_VISITOR_IMPL(TokenNode, AstNode)
    DEF_VISITOR_IMPL(IdentifierNode, AstNode)
    DEF_VISITOR_IMPL(CompilationUnitNode, AstNode)

    // Expressions
    DEF_VISITOR_IMPL(ExpressionNode, AstNode)
    DEF_VISITOR_IMPL(LiteralExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(IdentifierExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(ParenthesizedExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(UnaryExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(BinaryExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(AssignmentExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(CallExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(MemberAccessExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(NewExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(ThisExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(CastExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(IndexerExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(TypeOfExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(SizeOfExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(MatchExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(ConditionalExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(RangeExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(EnumMemberExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(FieldKeywordExpressionNode, ExpressionNode)
    DEF_VISITOR_IMPL(ValueKeywordExpressionNode, ExpressionNode)

    // Type Names
    DEF_VISITOR_IMPL(TypeNameNode, AstNode)
    DEF_VISITOR_IMPL(QualifiedTypeNameNode, TypeNameNode)
    DEF_VISITOR_IMPL(PointerTypeNameNode, TypeNameNode)
    DEF_VISITOR_IMPL(ArrayTypeNameNode, TypeNameNode)
    DEF_VISITOR_IMPL(GenericTypeNameNode, TypeNameNode)

    // Statements
    DEF_VISITOR_IMPL(StatementNode, AstNode)
    DEF_VISITOR_IMPL(EmptyStatementNode, StatementNode)
    DEF_VISITOR_IMPL(BlockStatementNode, StatementNode)
    DEF_VISITOR_IMPL(ExpressionStatementNode, StatementNode)
    DEF_VISITOR_IMPL(IfStatementNode, StatementNode)
    DEF_VISITOR_IMPL(WhileStatementNode, StatementNode)
    DEF_VISITOR_IMPL(ForStatementNode, StatementNode)
    DEF_VISITOR_IMPL(ReturnStatementNode, StatementNode)
    DEF_VISITOR_IMPL(BreakStatementNode, StatementNode)
    DEF_VISITOR_IMPL(ContinueStatementNode, StatementNode)
    DEF_VISITOR_IMPL(LocalVariableDeclarationNode, StatementNode)
    DEF_VISITOR_IMPL(UsingDirectiveNode, StatementNode)

    // Declarations
    DEF_VISITOR_IMPL(DeclarationNode, StatementNode)
    DEF_VISITOR_IMPL(NamespaceDeclarationNode, DeclarationNode)
    DEF_VISITOR_IMPL(TypeDeclarationNode, DeclarationNode)
    DEF_VISITOR_IMPL(InterfaceDeclarationNode, DeclarationNode)
    DEF_VISITOR_IMPL(EnumDeclarationNode, DeclarationNode)
    DEF_VISITOR_IMPL(MemberDeclarationNode, DeclarationNode)
    DEF_VISITOR_IMPL(FieldDeclarationNode, MemberDeclarationNode)
    DEF_VISITOR_IMPL(FunctionDeclarationNode, MemberDeclarationNode)
    DEF_VISITOR_IMPL(PropertyDeclarationNode, MemberDeclarationNode)
    DEF_VISITOR_IMPL(ConstructorDeclarationNode, MemberDeclarationNode)
    DEF_VISITOR_IMPL(EnumCaseNode, MemberDeclarationNode)
    DEF_VISITOR_IMPL(ParameterNode, DeclarationNode)
    DEF_VISITOR_IMPL(VariableDeclarationNode, DeclarationNode)
    DEF_VISITOR_IMPL(GenericParameterNode, DeclarationNode)
    
    // Match patterns
    DEF_VISITOR_IMPL(MatchArmNode, AstNode)
    DEF_VISITOR_IMPL(MatchPatternNode, AstNode)
    DEF_VISITOR_IMPL(EnumPatternNode, MatchPatternNode)
    DEF_VISITOR_IMPL(RangePatternNode, MatchPatternNode)
    DEF_VISITOR_IMPL(ComparisonPatternNode, MatchPatternNode)
    DEF_VISITOR_IMPL(WildcardPatternNode, MatchPatternNode)
    DEF_VISITOR_IMPL(LiteralPatternNode, MatchPatternNode)
    
    // Property accessor
    DEF_VISITOR_IMPL(PropertyAccessorNode, AstNode)
    
    #undef DEF_VISITOR_IMPL

    // --- Enum to_string Implementations ---
    // Moved to ast_enums.cpp

    // --- RTTI Utility Function Implementations ---

    const char* get_type_name_from_id(uint8_t type_id) {
        if (type_id < g_ordered_type_infos.size()) {
            return g_ordered_type_infos[type_id]->name;
        }
        return "UnknownType";
    }

    const char* get_node_type_name(const AstNode* node) {
        if (!node) {
            return "NullNode";
        }
        return get_type_name_from_id(node->typeId);
    }

    // --- Static Type Info Definitions ---
    // These define the static sTypeInfo members for AST nodes
    
    // Forward declare visitor accept functions
    void ErrorNode_accept(AstNode* node, StructuralVisitor* visitor);
    void ForInStatementNode_accept(AstNode* node, StructuralVisitor* visitor);
    
    // Type info definitions for new AST types
    AstTypeInfo ErrorNode::sTypeInfo("ErrorNode", &AstNode::sTypeInfo, ErrorNode_accept);
    AstTypeInfo ForInStatementNode::sTypeInfo("ForInStatementNode", &StatementNode::sTypeInfo, ForInStatementNode_accept);
    
    // Accept function implementations
    void ErrorNode_accept(AstNode* node, StructuralVisitor* visitor) {
        visitor->visit(static_cast<ErrorNode*>(node));
    }
    
    void ForInStatementNode_accept(AstNode* node, StructuralVisitor* visitor) {
        visitor->visit(static_cast<ForInStatementNode*>(node));
    }
    
    // StructuralVisitor method implementations for new types
    void StructuralVisitor::visit(ErrorNode* node) {
        // Default implementation - just report that we visited an error
        // Subclasses can override for specific error handling
    }
    
    void StructuralVisitor::visit(ForInStatementNode* node) {
        // Default implementation - visit children
        if (node->variable) visit(node->variable);
        if (node->iterable) visit(node->iterable);
        if (node->body) visit(node->body);
    }

} // namespace Mycelium::Scripting::Lang