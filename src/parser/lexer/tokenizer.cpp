#include "tokenizer.h"
#include <fstream>
#include <sstream>
#include <cctype>

namespace Monitor {
namespace Parser {
namespace Lexer {

// Constructor
Tokenizer::Tokenizer() {
    // Initialize with default options
}

// Main tokenization interface
std::vector<Token> Tokenizer::tokenize(const std::string& source) {
    clearErrors();
    TokenStream stream(source);
    std::vector<Token> tokens;

    while (stream.hasNext()) {
        Token token = stream.next();
        if (token.type != TokenType::INVALID) {
            tokens.push_back(token);
        }
    }

    return processTokens(tokens);
}

std::vector<Token> Tokenizer::tokenizeFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        addError("Cannot open file: " + filePath, 0, 0, 0);
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return tokenize(buffer.str());
}

std::vector<Token> Tokenizer::processTokens(const std::vector<Token>& rawTokens) {
    std::vector<Token> processedTokens;

    for (const auto& token : rawTokens) {
        // Apply options filtering
        if (token.type == TokenType::WHITESPACE && !m_options.includeWhitespace) {
            continue;
        }
        if (token.type == TokenType::COMMENT && !m_options.includeComments) {
            continue;
        }
        if (token.type == TokenType::NEWLINE && !m_options.includeNewlines) {
            continue;
        }

        processedTokens.push_back(token);
    }

    return processedTokens;
}

void Tokenizer::addError(const std::string& message, size_t line, size_t column, size_t position) {
    TokenizationError error;
    error.message = message;
    error.line = line;
    error.column = column;
    error.position = position;
    m_errors.push_back(error);
}

// TokenStream implementation
Tokenizer::TokenStream::TokenStream(const std::string& source)
    : m_source(source), m_position(0), m_line(1), m_column(1) {
}

bool Tokenizer::TokenStream::hasNext() const {
    return m_position < m_source.length();
}

Token Tokenizer::TokenStream::next() {
    return nextToken();
}

Token Tokenizer::TokenStream::peek() const {
    // Get next token using a temporary copy
    TokenStream temp = *this;
    Token token = temp.nextToken();
    return token;
}

void Tokenizer::TokenStream::reset() {
    m_position = 0;
    m_line = 1;
    m_column = 1;
}

Token Tokenizer::TokenStream::nextToken() {
    skipWhitespace();

    if (m_position >= m_source.length()) {
        return Token(TokenType::EOF_TOKEN, "", m_line, m_column, m_position);
    }

    char current = currentChar();
    size_t tokenLine = m_line;
    size_t tokenColumn = m_column;
    size_t tokenPosition = m_position;

    // Handle different token types
    if (current == '/') {
        char next = peekChar();
        if (next == '/' || next == '*') {
            skipComment();
            return Token(TokenType::COMMENT, "", tokenLine, tokenColumn, tokenPosition);
        }
        advance();
        return Token(TokenType::DIVIDE, "/", tokenLine, tokenColumn, tokenPosition);
    }

    if (current == '#') {
        // Handle preprocessor directives
        advance();
        std::string directive;
        while (hasNext() && isAlphaNumeric(currentChar())) {
            directive += currentChar();
            advance();
        }

        if (directive == "pragma") {
            return Token(TokenType::PRAGMA, "#pragma", tokenLine, tokenColumn, tokenPosition);
        } else if (directive == "include") {
            return Token(TokenType::INCLUDE, "#include", tokenLine, tokenColumn, tokenPosition);
        } else if (directive == "define") {
            return Token(TokenType::DEFINE, "#define", tokenLine, tokenColumn, tokenPosition);
        }
        return Token(TokenType::HASH, "#", tokenLine, tokenColumn, tokenPosition);
    }

    if (isAlpha(current) || current == '_') {
        return readIdentifierOrKeyword();
    }

    if (isDigit(current)) {
        return readNumber();
    }

    if (current == '"') {
        return readString();
    }

    if (current == '\'') {
        return readChar();
    }

    if (current == '\n') {
        advance();
        return Token(TokenType::NEWLINE, "\n", tokenLine, tokenColumn, tokenPosition);
    }

    // Handle operators and delimiters
    return readOperator();
}

char Tokenizer::TokenStream::currentChar() const {
    if (m_position >= m_source.length()) {
        return '\0';
    }
    return m_source[m_position];
}

char Tokenizer::TokenStream::peekChar(size_t offset) const {
    size_t pos = m_position + offset;
    if (pos >= m_source.length()) {
        return '\0';
    }
    return m_source[pos];
}

void Tokenizer::TokenStream::advance() {
    if (m_position < m_source.length()) {
        if (m_source[m_position] == '\n') {
            m_line++;
            m_column = 1;
        } else {
            m_column++;
        }
        m_position++;
    }
}

void Tokenizer::TokenStream::skipWhitespace() {
    while (hasNext() && isWhitespace(currentChar()) && currentChar() != '\n') {
        advance();
    }
}

void Tokenizer::TokenStream::skipComment() {
    if (currentChar() == '/' && peekChar() == '/') {
        // Single-line comment
        while (hasNext() && currentChar() != '\n') {
            advance();
        }
    } else if (currentChar() == '/' && peekChar() == '*') {
        // Multi-line comment
        advance(); // skip '/'
        advance(); // skip '*'

        while (hasNext()) {
            if (currentChar() == '*' && peekChar() == '/') {
                advance(); // skip '*'
                advance(); // skip '/'
                break;
            }
            advance();
        }
    }
}

Token Tokenizer::TokenStream::readIdentifierOrKeyword() {
    size_t tokenLine = m_line;
    size_t tokenColumn = m_column;
    size_t tokenPosition = m_position;
    std::string value;

    while (hasNext() && (isAlphaNumeric(currentChar()) || currentChar() == '_')) {
        value += currentChar();
        advance();
    }

    // Check if it's a keyword
    if (TokenTypeUtils::isKeyword(value)) {
        TokenType keywordType = TokenTypeUtils::getKeywordType(value);
        return Token(keywordType, value, tokenLine, tokenColumn, tokenPosition);
    }

    return Token(TokenType::IDENTIFIER, value, tokenLine, tokenColumn, tokenPosition);
}

Token Tokenizer::TokenStream::readNumber() {
    size_t tokenLine = m_line;
    size_t tokenColumn = m_column;
    size_t tokenPosition = m_position;
    std::string value;
    bool isFloat = false;

    // Handle hex numbers
    if (currentChar() == '0' && (peekChar() == 'x' || peekChar() == 'X')) {
        value += currentChar(); advance(); // '0'
        value += currentChar(); advance(); // 'x' or 'X'

        while (hasNext() && (isDigit(currentChar()) ||
               (std::tolower(currentChar()) >= 'a' && std::tolower(currentChar()) <= 'f'))) {
            value += currentChar();
            advance();
        }
        return Token(TokenType::INTEGER_LITERAL, value, tokenLine, tokenColumn, tokenPosition);
    }

    // Regular numbers
    while (hasNext() && isDigit(currentChar())) {
        value += currentChar();
        advance();
    }

    // Check for decimal point
    if (hasNext() && currentChar() == '.') {
        isFloat = true;
        value += currentChar();
        advance();

        while (hasNext() && isDigit(currentChar())) {
            value += currentChar();
            advance();
        }
    }

    // Check for exponential notation
    if (hasNext() && (currentChar() == 'e' || currentChar() == 'E')) {
        isFloat = true;
        value += currentChar();
        advance();

        if (hasNext() && (currentChar() == '+' || currentChar() == '-')) {
            value += currentChar();
            advance();
        }

        while (hasNext() && isDigit(currentChar())) {
            value += currentChar();
            advance();
        }
    }

    // Check for float/double suffixes
    if (hasNext() && (currentChar() == 'f' || currentChar() == 'F' ||
                     currentChar() == 'l' || currentChar() == 'L')) {
        isFloat = true;
        value += currentChar();
        advance();
    }

    TokenType type = isFloat ? TokenType::FLOAT_LITERAL : TokenType::INTEGER_LITERAL;
    return Token(type, value, tokenLine, tokenColumn, tokenPosition);
}

Token Tokenizer::TokenStream::readString() {
    size_t tokenLine = m_line;
    size_t tokenColumn = m_column;
    size_t tokenPosition = m_position;
    std::string value = "\"";

    advance(); // skip opening quote

    while (hasNext() && currentChar() != '"') {
        if (currentChar() == '\\') {
            value += currentChar();
            advance();
            if (hasNext()) {
                value += currentChar();
                advance();
            }
        } else {
            value += currentChar();
            advance();
        }
    }

    if (hasNext() && currentChar() == '"') {
        value += currentChar();
        advance();
    }

    return Token(TokenType::STRING_LITERAL, value, tokenLine, tokenColumn, tokenPosition);
}

Token Tokenizer::TokenStream::readChar() {
    size_t tokenLine = m_line;
    size_t tokenColumn = m_column;
    size_t tokenPosition = m_position;
    std::string value = "'";

    advance(); // skip opening quote

    while (hasNext() && currentChar() != '\'') {
        if (currentChar() == '\\') {
            value += currentChar();
            advance();
            if (hasNext()) {
                value += currentChar();
                advance();
            }
        } else {
            value += currentChar();
            advance();
        }
    }

    if (hasNext() && currentChar() == '\'') {
        value += currentChar();
        advance();
    }

    return Token(TokenType::CHAR_LITERAL, value, tokenLine, tokenColumn, tokenPosition);
}

Token Tokenizer::TokenStream::readOperator() {
    size_t tokenLine = m_line;
    size_t tokenColumn = m_column;
    size_t tokenPosition = m_position;

    char current = currentChar();
    char next = peekChar();

    // Two-character operators
    std::string twoChar = std::string(1, current) + std::string(1, next);
    const auto& operators = TokenTypeUtils::getOperators();

    auto twoCharIt = operators.find(twoChar);
    if (twoCharIt != operators.end()) {
        advance();
        advance();
        return Token(twoCharIt->second, twoChar, tokenLine, tokenColumn, tokenPosition);
    }

    // Single-character operators
    std::string oneChar = std::string(1, current);
    auto oneCharIt = operators.find(oneChar);
    if (oneCharIt != operators.end()) {
        advance();
        return Token(oneCharIt->second, oneChar, tokenLine, tokenColumn, tokenPosition);
    }

    // Unknown character
    advance();
    return Token(TokenType::INVALID, oneChar, tokenLine, tokenColumn, tokenPosition);
}

bool Tokenizer::TokenStream::isAlpha(char c) const {
    return std::isalpha(c) || c == '_';
}

bool Tokenizer::TokenStream::isDigit(char c) const {
    return std::isdigit(c);
}

bool Tokenizer::TokenStream::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

bool Tokenizer::TokenStream::isWhitespace(char c) const {
    return c == ' ' || c == '\t' || c == '\r';
}

} // namespace Lexer
} // namespace Parser
} // namespace Monitor