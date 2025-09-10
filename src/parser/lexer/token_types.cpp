#include "token_types.h"
#include <unordered_map>
#include <sstream>

namespace Monitor {
namespace Parser {
namespace Lexer {

bool Token::isKeyword() const {
    return TokenTypeUtils::isKeyword(value);
}

bool Token::isOperator() const {
    switch (type) {
        case TokenType::ASSIGN:
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::MULTIPLY:
        case TokenType::DIVIDE:
        case TokenType::MODULO:
        case TokenType::BITWISE_AND:
        case TokenType::BITWISE_OR:
        case TokenType::BITWISE_XOR:
        case TokenType::BITWISE_NOT:
        case TokenType::LOGICAL_AND:
        case TokenType::LOGICAL_OR:
        case TokenType::LOGICAL_NOT:
        case TokenType::EQUAL:
        case TokenType::NOT_EQUAL:
        case TokenType::LESS_THAN:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER_THAN:
        case TokenType::GREATER_EQUAL:
            return true;
        default:
            return false;
    }
}

bool Token::isLiteral() const {
    switch (type) {
        case TokenType::IDENTIFIER:
        case TokenType::INTEGER_LITERAL:
        case TokenType::FLOAT_LITERAL:
        case TokenType::STRING_LITERAL:
        case TokenType::CHAR_LITERAL:
            return true;
        default:
            return false;
    }
}

bool Token::isDelimiter() const {
    switch (type) {
        case TokenType::SEMICOLON:
        case TokenType::COMMA:
        case TokenType::COLON:
        case TokenType::DOUBLE_COLON:
        case TokenType::DOT:
        case TokenType::ARROW:
        case TokenType::LEFT_PAREN:
        case TokenType::RIGHT_PAREN:
        case TokenType::LEFT_BRACE:
        case TokenType::RIGHT_BRACE:
        case TokenType::LEFT_BRACKET:
        case TokenType::RIGHT_BRACKET:
            return true;
        default:
            return false;
    }
}

bool Token::isType() const {
    switch (type) {
        case TokenType::VOID:
        case TokenType::CHAR:
        case TokenType::SHORT:
        case TokenType::INT:
        case TokenType::LONG:
        case TokenType::FLOAT:
        case TokenType::DOUBLE:
        case TokenType::SIGNED:
        case TokenType::UNSIGNED:
            return true;
        default:
            return false;
    }
}

std::string Token::toString() const {
    std::stringstream ss;
    ss << "Token(" << TokenTypeUtils::tokenTypeToString(type) 
       << ", \"" << value << "\", " << line << ":" << column << ")";
    return ss.str();
}

// Static keyword map
const std::unordered_map<std::string, TokenType>& TokenTypeUtils::getKeywords() {
    static const std::unordered_map<std::string, TokenType> keywords = {
        // Structure keywords
        {"typedef", TokenType::TYPEDEF},
        {"struct", TokenType::STRUCT},
        {"union", TokenType::UNION},
        {"class", TokenType::CLASS},
        {"enum", TokenType::ENUM},
        
        // Type qualifiers
        {"const", TokenType::CONST},
        {"volatile", TokenType::VOLATILE},
        {"static", TokenType::STATIC},
        {"extern", TokenType::EXTERN},
        {"inline", TokenType::INLINE},
        
        // Primitive types
        {"void", TokenType::VOID},
        {"char", TokenType::CHAR},
        {"short", TokenType::SHORT},
        {"int", TokenType::INT},
        {"long", TokenType::LONG},
        {"float", TokenType::FLOAT},
        {"double", TokenType::DOUBLE},
        {"signed", TokenType::SIGNED},
        {"unsigned", TokenType::UNSIGNED},
        
        // Attributes
        {"__attribute__", TokenType::ATTRIBUTE},
        {"packed", TokenType::PACKED},
    };
    return keywords;
}

// Static operator map
const std::unordered_map<std::string, TokenType>& TokenTypeUtils::getOperators() {
    static const std::unordered_map<std::string, TokenType> operators = {
        {"=", TokenType::ASSIGN},
        {"+", TokenType::PLUS},
        {"-", TokenType::MINUS},
        {"*", TokenType::MULTIPLY},
        {"/", TokenType::DIVIDE},
        {"%", TokenType::MODULO},
        {"&", TokenType::BITWISE_AND},
        {"|", TokenType::BITWISE_OR},
        {"^", TokenType::BITWISE_XOR},
        {"~", TokenType::BITWISE_NOT},
        {"&&", TokenType::LOGICAL_AND},
        {"||", TokenType::LOGICAL_OR},
        {"!", TokenType::LOGICAL_NOT},
        {"==", TokenType::EQUAL},
        {"!=", TokenType::NOT_EQUAL},
        {"<", TokenType::LESS_THAN},
        {"<=", TokenType::LESS_EQUAL},
        {">", TokenType::GREATER_THAN},
        {">=", TokenType::GREATER_EQUAL},
        {";", TokenType::SEMICOLON},
        {",", TokenType::COMMA},
        {":", TokenType::COLON},
        {"::", TokenType::DOUBLE_COLON},
        {".", TokenType::DOT},
        {"->", TokenType::ARROW},
        {"(", TokenType::LEFT_PAREN},
        {")", TokenType::RIGHT_PAREN},
        {"{", TokenType::LEFT_BRACE},
        {"}", TokenType::RIGHT_BRACE},
        {"[", TokenType::LEFT_BRACKET},
        {"]", TokenType::RIGHT_BRACKET},
        {"#", TokenType::HASH},
    };
    return operators;
}

std::string TokenTypeUtils::tokenTypeToString(TokenType type) {
    switch (type) {
        // Literals
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INTEGER_LITERAL: return "INTEGER_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::CHAR_LITERAL: return "CHAR_LITERAL";
        
        // Keywords
        case TokenType::TYPEDEF: return "TYPEDEF";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::UNION: return "UNION";
        case TokenType::CLASS: return "CLASS";
        case TokenType::ENUM: return "ENUM";
        case TokenType::CONST: return "CONST";
        case TokenType::VOLATILE: return "VOLATILE";
        case TokenType::STATIC: return "STATIC";
        case TokenType::EXTERN: return "EXTERN";
        case TokenType::INLINE: return "INLINE";
        
        // Types
        case TokenType::VOID: return "VOID";
        case TokenType::CHAR: return "CHAR";
        case TokenType::SHORT: return "SHORT";
        case TokenType::INT: return "INT";
        case TokenType::LONG: return "LONG";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::DOUBLE: return "DOUBLE";
        case TokenType::SIGNED: return "SIGNED";
        case TokenType::UNSIGNED: return "UNSIGNED";
        
        // Operators
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::MULTIPLY: return "MULTIPLY";
        case TokenType::DIVIDE: return "DIVIDE";
        case TokenType::MODULO: return "MODULO";
        case TokenType::BITWISE_AND: return "BITWISE_AND";
        case TokenType::BITWISE_OR: return "BITWISE_OR";
        case TokenType::BITWISE_XOR: return "BITWISE_XOR";
        case TokenType::BITWISE_NOT: return "BITWISE_NOT";
        case TokenType::LOGICAL_AND: return "LOGICAL_AND";
        case TokenType::LOGICAL_OR: return "LOGICAL_OR";
        case TokenType::LOGICAL_NOT: return "LOGICAL_NOT";
        
        // Comparison
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::LESS_THAN: return "LESS_THAN";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER_THAN: return "GREATER_THAN";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        
        // Delimiters
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::COLON: return "COLON";
        case TokenType::DOUBLE_COLON: return "DOUBLE_COLON";
        case TokenType::DOT: return "DOT";
        case TokenType::ARROW: return "ARROW";
        
        // Brackets
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        
        // Preprocessor
        case TokenType::PRAGMA: return "PRAGMA";
        case TokenType::INCLUDE: return "INCLUDE";
        case TokenType::DEFINE: return "DEFINE";
        case TokenType::HASH: return "HASH";
        
        // Special
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::WHITESPACE: return "WHITESPACE";
        case TokenType::COMMENT: return "COMMENT";
        case TokenType::EOF_TOKEN: return "EOF_TOKEN";
        case TokenType::INVALID: return "INVALID";
        
        // Attributes
        case TokenType::ATTRIBUTE: return "ATTRIBUTE";
        case TokenType::PACKED: return "PACKED";
        case TokenType::ALIGNED: return "ALIGNED";
        
        default: return "UNKNOWN";
    }
}

bool TokenTypeUtils::isKeyword(const std::string& identifier) {
    const auto& keywords = getKeywords();
    return keywords.find(identifier) != keywords.end();
}

TokenType TokenTypeUtils::getKeywordType(const std::string& keyword) {
    const auto& keywords = getKeywords();
    auto it = keywords.find(keyword);
    return it != keywords.end() ? it->second : TokenType::INVALID;
}

} // namespace Lexer
} // namespace Parser
} // namespace Monitor