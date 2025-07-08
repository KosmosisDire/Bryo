#pragma once

#include "parser/parser_context.hpp"
#include <optional>
#include <vector>
#include <functional>

namespace Mycelium::Scripting::Parser {

// Forward declarations
struct ParserDiagnostic;

// Result type for parser operations that can succeed with a value or fail with errors
template<typename T>
class ParseResult {
private:
    std::optional<T> value_;
    std::vector<ParserDiagnostic> errors_;
    bool recovered_;

public:
    // Constructors
    ParseResult() : recovered_(false) {}
    
    explicit ParseResult(T&& value) 
        : value_(std::move(value)), recovered_(false) {}
    
    explicit ParseResult(const T& value) 
        : value_(value), recovered_(false) {}
        
    explicit ParseResult(std::vector<ParserDiagnostic> errors) 
        : errors_(std::move(errors)), recovered_(false) {}
        
    explicit ParseResult(ParserDiagnostic error) 
        : recovered_(false) {
        errors_.push_back(std::move(error));
    }

    // Copy and move constructors
    ParseResult(const ParseResult&) = default;
    ParseResult(ParseResult&&) = default;
    ParseResult& operator=(const ParseResult&) = default;
    ParseResult& operator=(ParseResult&&) = default;

    // Status checking
    bool has_value() const { return value_.has_value(); }
    bool has_errors() const { return !errors_.empty(); }
    bool is_recovered() const { return recovered_; }
    bool is_success() const { return has_value() && !has_errors(); }
    bool is_failure() const { return !has_value() && has_errors(); }
    bool is_partial() const { return has_value() && has_errors(); }

    // Value access
    T& value() { 
        if (!has_value()) {
            throw std::runtime_error("ParseResult: Attempted to access value when none exists");
        }
        return value_.value(); 
    }
    
    const T& value() const { 
        if (!has_value()) {
            throw std::runtime_error("ParseResult: Attempted to access value when none exists");
        }
        return value_.value(); 
    }
    
    T value_or(T default_value) const {
        return has_value() ? value_.value() : std::move(default_value);
    }

    // Error access
    const std::vector<ParserDiagnostic>& errors() const { return errors_; }
    std::vector<ParserDiagnostic>& errors() { return errors_; }
    
    // Add more errors
    ParseResult& add_error(ParserDiagnostic error) {
        errors_.push_back(std::move(error));
        return *this;
    }
    
    ParseResult& add_errors(const std::vector<ParserDiagnostic>& new_errors) {
        errors_.insert(errors_.end(), new_errors.begin(), new_errors.end());
        return *this;
    }

    // Recovery management
    ParseResult& mark_recovered() {
        recovered_ = true;
        return *this;
    }

    // Monadic operations for chaining
    template<typename F>
    auto and_then(F&& func) -> decltype(func(std::declval<T>())) {
        using ResultType = decltype(func(std::declval<T>()));
        
        if (!has_value()) {
            // Propagate errors
            return ResultType::errors(errors_);
        }
        
        auto result = func(value_.value());
        
        // Combine errors from both results
        if (has_errors()) {
            result.add_errors(errors_);
        }
        
        return result;
    }
    
    // Error recovery operation
    ParseResult or_else(std::function<ParseResult<T>()> recovery_func) {
        if (has_value()) {
            return *this; // Already successful
        }
        
        auto recovery_result = recovery_func();
        
        // Combine errors from both attempts
        recovery_result.add_errors(errors_);
        
        if (recovery_result.has_value()) {
            recovery_result.mark_recovered();
        }
        
        return recovery_result;
    }
    
    // Type casting for inheritance hierarchies
    template<typename U>
    ParseResult<U> cast() const {
        static_assert(std::is_pointer_v<T> && std::is_pointer_v<U>, 
                     "cast() can only be used with pointer types");
        
        if (!has_value()) {
            return ParseResult<U>::errors(errors_);
        }
        
        // Perform the cast
        U casted_value = static_cast<U>(value_.value());
        auto result = ParseResult<U>::success(casted_value);
        
        // Preserve errors and recovery status
        if (has_errors()) {
            result.add_errors(errors_);
        }
        if (recovered_) {
            result.mark_recovered();
        }
        
        return result;
    }
    
    // Transform the value if present, preserve errors
    template<typename F>
    auto map(F&& func) -> ParseResult<std::decay_t<decltype(func(std::declval<T>()))>> {
        using U = std::decay_t<decltype(func(std::declval<T>()))>;
        
        if (!has_value()) {
            return ParseResult<U>::errors(errors_);
        }
        
        auto mapped_value = func(value_.value());
        auto result = ParseResult<U>::success(std::move(mapped_value));
        result.add_errors(errors_);
        
        if (recovered_) {
            result.mark_recovered();
        }
        
        return result;
    }

    // Static factory methods
    static ParseResult<T> success(T&& value) {
        return ParseResult<T>(std::move(value));
    }
    
    static ParseResult<T> success(const T& value) {
        return ParseResult<T>(value);
    }
    
    static ParseResult<T> error(ParserDiagnostic diagnostic) {
        return ParseResult<T>(std::move(diagnostic));
    }
    
    static ParseResult<T> errors(std::vector<ParserDiagnostic> diagnostics) {
        return ParseResult<T>(std::move(diagnostics));
    }
    
    static ParseResult<T> partial_success(T&& value, std::vector<ParserDiagnostic> errors) {
        ParseResult<T> result(std::move(value));
        result.add_errors(errors);
        return result;
    }
};

// Helper functions for combining ParseResults

// Combine two ParseResults, preferring the first success
template<typename T>
ParseResult<T> combine_results(ParseResult<T> first, ParseResult<T> second) {
    if (first.has_value()) {
        first.add_errors(second.errors());
        return first;
    }
    
    if (second.has_value()) {
        second.add_errors(first.errors());
        return second;
    }
    
    // Both failed, combine errors
    first.add_errors(second.errors());
    return first;
}

// Collect multiple ParseResults into a vector result
template<typename T>
ParseResult<std::vector<T>> collect_results(std::vector<ParseResult<T>> results) {
    std::vector<T> values;
    std::vector<ParserDiagnostic> all_errors;
    bool any_recovered = false;
    
    for (auto& result : results) {
        if (result.has_value()) {
            values.push_back(std::move(result.value()));
        }
        
        if (result.has_errors()) {
            all_errors.insert(all_errors.end(), 
                            result.errors().begin(), result.errors().end());
        }
        
        if (result.is_recovered()) {
            any_recovered = true;
        }
    }
    
    if (values.empty()) {
        return ParseResult<std::vector<T>>::errors(std::move(all_errors));
    }
    
    auto result = ParseResult<std::vector<T>>::success(std::move(values));
    result.add_errors(all_errors);
    
    if (any_recovered) {
        result.mark_recovered();
    }
    
    return result;
}

// Convenience macros for common patterns
#define PARSE_TRY(expr) \
    ({ \
        auto _result = (expr); \
        if (!_result.has_value()) return ParseResult<decltype(_result.value())>::errors(_result.errors()); \
        _result.value(); \
    })

#define PARSE_TRY_OR_RECOVER(expr, recovery) \
    ({ \
        auto _result = (expr); \
        if (!_result.has_value()) { \
            _result = _result.or_else([&]() { return (recovery); }); \
        } \
        _result; \
    })

} // namespace Mycelium::Scripting::Parser