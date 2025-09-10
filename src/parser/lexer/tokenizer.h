#pragma once

#include "token_types.h"
#include <vector>
#include <string>
#include <memory>

namespace Monitor {
namespace Parser {
namespace Lexer {

class Tokenizer {
public:
    explicit Tokenizer();
    ~Tokenizer() = default;
    
    // Main tokenization interface
    std::vector<Token> tokenize(const std::string& source);
    std::vector<Token> tokenizeFile(const std::string& filePath);
    
    // Streaming interface for large files
    class TokenStream {
    public:
        explicit TokenStream(const std::string& source);
        ~TokenStream() = default;
        
        bool hasNext() const;
        Token next();
        Token peek() const;
        void reset();
        
        size_t getPosition() const { return m_position; }
        size_t getLine() const { return m_line; }
        size_t getColumn() const { return m_column; }
        
    private:
        std::string m_source;
        size_t m_position;
        size_t m_line;
        size_t m_column;
        
        Token nextToken();
        char currentChar() const;
        char peekChar(size_t offset = 1) const;
        void advance();
        void skipWhitespace();
        void skipComment();
        Token readIdentifierOrKeyword();
        Token readNumber();
        Token readString();
        Token readChar();
        Token readOperator();
        bool isAlpha(char c) const;
        bool isDigit(char c) const;
        bool isAlphaNumeric(char c) const;
        bool isWhitespace(char c) const;
    };
    
    // Configuration options
    struct TokenizerOptions {
        bool includeWhitespace = false;
        bool includeComments = false;
        bool includeNewlines = false;
        bool mergeContinuousWhitespace = true;
    };
    
    void setOptions(const TokenizerOptions& options) { m_options = options; }
    const TokenizerOptions& getOptions() const { return m_options; }
    
    // Error handling
    struct TokenizationError {
        std::string message;
        size_t line;
        size_t column;
        size_t position;
    };
    
    const std::vector<TokenizationError>& getErrors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors() { m_errors.clear(); }
    
private:
    TokenizerOptions m_options;
    std::vector<TokenizationError> m_errors;
    
    void addError(const std::string& message, size_t line, size_t column, size_t position);
    std::vector<Token> processTokens(const std::vector<Token>& rawTokens);
};

} // namespace Lexer
} // namespace Parser
} // namespace Monitor