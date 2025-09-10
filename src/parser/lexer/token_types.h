#pragma once

#include <string>
#include <unordered_map>

namespace Monitor {
namespace Parser {
namespace Lexer {

enum class TokenType {
    // Literals
    IDENTIFIER,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,
    
    // Keywords
    TYPEDEF,
    STRUCT,
    UNION,
    CLASS,
    ENUM,
    CONST,
    VOLATILE,
    STATIC,
    EXTERN,
    INLINE,
    
    // Types
    VOID,
    CHAR,
    SHORT,
    INT,
    LONG,
    FLOAT,
    DOUBLE,
    SIGNED,
    UNSIGNED,
    
    // Operators
    ASSIGN,          // =
    PLUS,           // +
    MINUS,          // -
    MULTIPLY,       // *
    DIVIDE,         // /
    MODULO,         // %
    BITWISE_AND,    // &
    BITWISE_OR,     // |
    BITWISE_XOR,    // ^
    BITWISE_NOT,    // ~
    LOGICAL_AND,    // &&
    LOGICAL_OR,     // ||
    LOGICAL_NOT,    // !
    
    // Comparison
    EQUAL,          // ==
    NOT_EQUAL,      // !=
    LESS_THAN,      // <
    LESS_EQUAL,     // <=
    GREATER_THAN,   // >
    GREATER_EQUAL,  // >=
    
    // Delimiters
    SEMICOLON,      // ;
    COMMA,          // ,
    COLON,          // :
    DOUBLE_COLON,   // ::
    DOT,            // .
    ARROW,          // ->
    
    // Brackets
    LEFT_PAREN,     // (
    RIGHT_PAREN,    // )
    LEFT_BRACE,     // {
    RIGHT_BRACE,    // }
    LEFT_BRACKET,   // [
    RIGHT_BRACKET,  // ]
    
    // Preprocessor
    PRAGMA,         // #pragma
    INCLUDE,        // #include
    DEFINE,         // #define
    HASH,           // #
    
    // Special
    NEWLINE,
    WHITESPACE,
    COMMENT,
    EOF_TOKEN,
    INVALID,
    
    // Attributes (GCC/Clang)
    ATTRIBUTE,      // __attribute__
    PACKED,         // packed
    ALIGNED,        // aligned
};

struct Token {
    TokenType type;
    std::string value;
    size_t line;
    size_t column;
    size_t position;
    
    Token() : type(TokenType::INVALID), line(0), column(0), position(0) {}
    
    Token(TokenType t, const std::string& v, size_t l, size_t c, size_t p)
        : type(t), value(v), line(l), column(c), position(p) {}
    
    bool isKeyword() const;
    bool isOperator() const;
    bool isLiteral() const;
    bool isDelimiter() const;
    bool isType() const;
    
    std::string toString() const;
};

// Token type utilities
class TokenTypeUtils {
public:
    static const std::unordered_map<std::string, TokenType>& getKeywords();
    static const std::unordered_map<std::string, TokenType>& getOperators();
    static std::string tokenTypeToString(TokenType type);
    static bool isKeyword(const std::string& identifier);
    static TokenType getKeywordType(const std::string& keyword);
};

} // namespace Lexer
} // namespace Parser
} // namespace Monitor