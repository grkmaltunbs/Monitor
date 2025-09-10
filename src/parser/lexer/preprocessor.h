#pragma once

#include "token_types.h"
#include <vector>
#include <stack>
#include <unordered_map>
#include <string>

namespace Monitor {
namespace Parser {
namespace Lexer {

class Preprocessor {
public:
    explicit Preprocessor();
    ~Preprocessor() = default;
    
    // Preprocessor directive types
    enum class DirectiveType {
        PRAGMA_PACK_PUSH,
        PRAGMA_PACK_POP,
        PRAGMA_PACK_SET,
        PRAGMA_PACK_RESET,
        INCLUDE,
        DEFINE,
        UNDEF,
        IFDEF,
        IFNDEF,
        ENDIF,
        UNKNOWN
    };
    
    struct Directive {
        DirectiveType type;
        std::string name;
        std::vector<std::string> arguments;
        size_t line;
        size_t column;
        
        Directive(DirectiveType t, const std::string& n, size_t l, size_t c)
            : type(t), name(n), line(l), column(c) {}
    };
    
    // Pack state management
    struct PackState {
        uint8_t packValue;
        std::string identifier;  // Optional identifier for push/pop
        
        PackState(uint8_t value = 0, const std::string& id = "")
            : packValue(value), identifier(id) {}
    };
    
    // Main preprocessing interface
    std::vector<Token> process(const std::vector<Token>& tokens);
    
    // Pack state queries
    uint8_t getCurrentPackValue() const;
    bool isPackActive() const;
    const std::stack<PackState>& getPackStack() const { return m_packStack; }
    
    // Directive processing
    bool processDirective(const std::vector<Token>& directiveTokens);
    DirectiveType identifyDirective(const std::string& directiveName) const;
    
    // Macro support (basic)
    void defineMacro(const std::string& name, const std::string& value);
    void undefineMacro(const std::string& name);
    bool isMacroDefined(const std::string& name) const;
    std::string getMacroValue(const std::string& name) const;
    
    // Error handling
    struct PreprocessorError {
        std::string message;
        size_t line;
        size_t column;
        std::string directiveName;
    };
    
    const std::vector<PreprocessorError>& getErrors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors() { m_errors.clear(); }
    
    // Reset state
    void reset();
    
private:
    std::stack<PackState> m_packStack;
    std::unordered_map<std::string, std::string> m_macros;
    std::vector<PreprocessorError> m_errors;
    std::vector<Directive> m_directives;
    
    // Default pack value (platform-specific)
    uint8_t m_defaultPackValue;
    
    // Directive processors
    bool processPragmaPack(const std::vector<std::string>& args, size_t line, size_t column);
    bool processInclude(const std::vector<std::string>& args, size_t line, size_t column);
    bool processDefine(const std::vector<std::string>& args, size_t line, size_t column);
    
    // Utility functions
    std::vector<std::string> parseArguments(const std::vector<Token>& tokens, size_t startIndex);
    bool isValidPackValue(uint8_t value) const;
    void addError(const std::string& message, size_t line, size_t column, const std::string& directive = "");
    
    // Platform-specific initialization
    void initializePlatformDefaults();
};

} // namespace Lexer
} // namespace Parser
} // namespace Monitor