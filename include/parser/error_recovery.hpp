#pragma once

#include "parser/token_stream.hpp"
#include "parser/parser_context.hpp"
#include <vector>
#include <unordered_set>
#include <functional>

namespace Mycelium::Scripting::Parser {

// Forward declaration
class ParserBase;

// Error recovery strategies
enum class RecoveryStrategy {
    SkipToken,          // Skip the current token and continue
    InsertToken,        // Insert a missing token
    ReplaceToken,       // Replace current token with expected
    Synchronize,        // Find synchronization point
    BacktrackAndRetry,  // Backtrack and try alternative
    SkipToDelimiter,    // Skip to matching delimiter
    FailAndPropagate    // Give up and let caller handle
};

// Recovery action result
struct RecoveryResult {
    bool success;
    RecoveryStrategy strategy_used;
    std::string description;
    
    RecoveryResult(bool s, RecoveryStrategy strat, const std::string& desc = "")
        : success(s), strategy_used(strat), description(desc) {}
    
    static RecoveryResult success_with(RecoveryStrategy strategy, const std::string& desc = "") {
        return RecoveryResult(true, strategy, desc);
    }
    
    static RecoveryResult failure() {
        return RecoveryResult(false, RecoveryStrategy::FailAndPropagate, "Recovery failed");
    }
};

// Synchronization points for different language constructs
struct SynchronizationPoints {
    std::unordered_set<TokenKind> statement_sync = {
        TokenKind::Semicolon,
        TokenKind::LeftBrace,
        TokenKind::RightBrace,
        TokenKind::If,
        TokenKind::While,
        TokenKind::For,
        TokenKind::Return,
        TokenKind::Break,
        TokenKind::Continue
    };
    
    std::unordered_set<TokenKind> declaration_sync = {
        TokenKind::Fn,
        TokenKind::Type,
        TokenKind::Interface,
        TokenKind::Enum,
        TokenKind::Using,
        TokenKind::Namespace,
        TokenKind::Public,
        TokenKind::Private,
        TokenKind::Protected,
        TokenKind::Static
    };
    
    std::unordered_set<TokenKind> expression_sync = {
        TokenKind::Semicolon,
        TokenKind::Comma,
        TokenKind::RightParen,
        TokenKind::RightBrace,
        TokenKind::RightBracket
    };
    
    std::unordered_set<TokenKind> block_sync = {
        TokenKind::LeftBrace,
        TokenKind::RightBrace
    };
    
    std::unordered_set<TokenKind> parameter_sync = {
        TokenKind::Comma,
        TokenKind::RightParen,
        TokenKind::Arrow
    };
};

// Error production patterns for common mistakes
struct ErrorProduction {
    std::vector<TokenKind> pattern;
    TokenKind suggested_replacement;
    std::string message;
    
    ErrorProduction(std::initializer_list<TokenKind> pat, TokenKind replacement, const std::string& msg)
        : pattern(pat), suggested_replacement(replacement), message(msg) {}
};

// Main error recovery class
class ErrorRecovery {
public:
    explicit ErrorRecovery(SynchronizationPoints sync_points = {});
    
    // Core recovery methods
    RecoveryResult recover_from_error(ParserBase& parser, 
                                    TokenKind expected,
                                    ParsingContext context);
    
    RecoveryResult recover_from_error(ParserBase& parser,
                                    std::initializer_list<TokenKind> expected,
                                    ParsingContext context);
    
    // Specific recovery strategies
    RecoveryResult panic_mode_recovery(TokenStream& tokens, 
                                     ParserContext& context,
                                     ParsingContext parsing_context);
    
    RecoveryResult phrase_level_recovery(TokenStream& tokens,
                                       ParserContext& context,
                                       TokenKind expected);
    
    RecoveryResult error_production_recovery(TokenStream& tokens,
                                           ParserContext& context,
                                           ParsingContext parsing_context);
    
    RecoveryResult delimiter_matching_recovery(TokenStream& tokens,
                                             ParserContext& context,
                                             TokenKind expected_delimiter);
    
    // Backtracking-based recovery
    RecoveryResult backtrack_recovery(ParserBase& parser,
                                    std::function<bool()> alternative_parser);
    
    // Synchronization helpers
    bool synchronize_to_statement(TokenStream& tokens);
    bool synchronize_to_declaration(TokenStream& tokens);
    bool synchronize_to_expression_end(TokenStream& tokens);
    bool synchronize_to_block_end(TokenStream& tokens);
    
    // Error production detection
    bool try_common_error_patterns(TokenStream& tokens, ParserContext& context);
    bool try_missing_semicolon(TokenStream& tokens, ParserContext& context);
    bool try_missing_comma(TokenStream& tokens, ParserContext& context);
    bool try_mismatched_delimiters(TokenStream& tokens, ParserContext& context);
    bool try_keyword_as_identifier(TokenStream& tokens, ParserContext& context);
    
    // Configuration
    void add_error_production(const ErrorProduction& production);
    void set_max_recovery_attempts(int max_attempts) { max_recovery_attempts_ = max_attempts; }
    void set_aggressive_recovery(bool aggressive) { aggressive_recovery_ = aggressive; }
    
    // Statistics and diagnostics
    int recovery_attempts() const { return recovery_attempts_; }
    int successful_recoveries() const { return successful_recoveries_; }
    void reset_statistics();

private:
    SynchronizationPoints sync_points_;
    std::vector<ErrorProduction> error_productions_;
    int max_recovery_attempts_;
    bool aggressive_recovery_;
    
    // Statistics
    mutable int recovery_attempts_;
    mutable int successful_recoveries_;
    
    // Helper methods
    std::unordered_set<TokenKind> get_sync_tokens_for_context(ParsingContext context) const;
    bool is_likely_sync_point(TokenKind token, ParsingContext context) const;
    bool is_delimiter_pair(TokenKind open, TokenKind close) const;
    TokenKind find_matching_delimiter(TokenKind open) const;
    
    // Recovery strategy selection
    RecoveryStrategy select_best_strategy(TokenStream& tokens,
                                        ParsingContext context,
                                        TokenKind expected) const;
    
    RecoveryStrategy select_best_strategy(TokenStream& tokens,
                                        ParsingContext context,
                                        std::initializer_list<TokenKind> expected) const;
    
    // Recovery execution
    RecoveryResult execute_strategy(ParserBase& parser,
                                  RecoveryStrategy strategy,
                                  TokenKind expected,
                                  ParsingContext context);
    
    // Advanced recovery techniques
    bool try_insertion_recovery(TokenStream& tokens, 
                              ParserContext& context,
                              TokenKind missing_token);
    
    bool try_replacement_recovery(TokenStream& tokens,
                                ParserContext& context,
                                TokenKind expected_token);
    
    bool try_deletion_recovery(TokenStream& tokens,
                             ParserContext& context);
    
    // Lookahead analysis for recovery
    bool analyze_recovery_potential(TokenStream& tokens,
                                  ParsingContext context,
                                  int lookahead_distance = 5) const;
    
    // Error clustering to avoid cascading errors
    bool is_likely_cascading_error(TokenStream& tokens, ParsingContext context) const;
    void suppress_cascading_errors(ParserContext& context) const;
};

// Built-in error productions for common mistakes
namespace CommonErrors {
    extern const std::vector<ErrorProduction> MISSING_SEMICOLON;
    extern const std::vector<ErrorProduction> MISSING_COMMA;
    extern const std::vector<ErrorProduction> MISMATCHED_DELIMITERS;
    extern const std::vector<ErrorProduction> KEYWORD_AS_IDENTIFIER;
    extern const std::vector<ErrorProduction> ASSIGNMENT_VS_EQUALITY;
    extern const std::vector<ErrorProduction> MISSING_COLON_IN_TYPE;
}

// Utility functions for error recovery

// Check if a token sequence looks like a valid recovery point
bool is_good_recovery_point(const TokenStream& tokens, 
                           ParsingContext context,
                           int lookahead = 3);

// Estimate the "cost" of a recovery strategy
int estimate_recovery_cost(RecoveryStrategy strategy, 
                          int tokens_to_skip,
                          ParsingContext context);

// Find the best synchronization point within a given distance
std::optional<int> find_best_sync_point(const TokenStream& tokens,
                                       ParsingContext context,
                                       int max_distance = 10);

// Check if the current error is likely to be resolved by a simple recovery
bool is_simple_recoverable_error(TokenKind current, 
                                TokenKind expected,
                                ParsingContext context);

// Generate helpful error messages based on context
std::string generate_recovery_suggestion(RecoveryStrategy strategy,
                                        TokenKind current,
                                        TokenKind expected,
                                        ParsingContext context);

} // namespace Mycelium::Scripting::Parser