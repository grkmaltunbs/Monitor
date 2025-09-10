#pragma once

#include "../ast/ast_nodes.h"
#include "../ast/ast_builder.h"
#include "../lexer/token_types.h"
#include "../lexer/tokenizer.h"
#include "../lexer/preprocessor.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>

namespace Monitor {
namespace Parser {

/**
 * @brief Main C++ structure parser
 * 
 * This class implements a recursive descent parser for C++ structure
 * definitions. It handles complex constructs including bitfields, unions,
 * nested structures, and pragma directives.
 */
class StructParser {
public:
    explicit StructParser();
    ~StructParser() = default;
    
    // Main parsing interface
    struct ParseResult {
        bool success = false;
        std::vector<std::unique_ptr<AST::StructDeclaration>> structures;
        std::vector<std::unique_ptr<AST::UnionDeclaration>> unions;
        std::vector<std::unique_ptr<AST::TypedefDeclaration>> typedefs;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        size_t totalNodes = 0;
        std::chrono::milliseconds parseTime{0};
    };
    
    ParseResult parse(const std::string& source);
    ParseResult parseFromFile(const std::string& filePath);
    ParseResult parseFromTokens(const std::vector<Lexer::Token>& tokens);
    
    // Configuration options
    struct ParserOptions {
        bool strictMode = true;              // Strict C++ compliance
        bool allowNestedStructs = true;      // Allow struct definitions inside structs
        bool allowAnonymousStructs = true;   // Allow unnamed structs/unions
        bool handlePragmaPack = true;        // Process #pragma pack directives
        bool handleAttributes = true;        // Process __attribute__ directives
        bool allowForwardDeclarations = true; // Allow incomplete type references
        size_t maxNestingDepth = 32;         // Maximum nesting level
        size_t maxFieldsPerStruct = 1000;    // Safety limit
    };
    
    void setOptions(const ParserOptions& options) { m_options = options; }
    const ParserOptions& getOptions() const { return m_options; }
    
    // Error handling
    struct ParseError {
        std::string message;
        size_t line;
        size_t column;
        size_t position;
        std::string context;
        
        ParseError(const std::string& msg, size_t l, size_t c, size_t p, const std::string& ctx = "")
            : message(msg), line(l), column(c), position(p), context(ctx) {}
        
        std::string toString() const;
    };
    
    const std::vector<ParseError>& getErrors() const { return m_errors; }
    const std::vector<ParseError>& getWarnings() const { return m_warnings; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors() { m_errors.clear(); m_warnings.clear(); }
    
    // Statistics
    struct ParseStatistics {
        size_t tokensProcessed = 0;
        size_t structsParsed = 0;
        size_t unionsParsed = 0;
        size_t fieldsParsed = 0;
        size_t bitfieldsParsed = 0;
        size_t typedefsParsed = 0;
        size_t pragmasProcessed = 0;
        std::chrono::milliseconds tokenizationTime{0};
        std::chrono::milliseconds preprocessingTime{0};
        std::chrono::milliseconds parsingTime{0};
        std::chrono::milliseconds totalTime{0};
    };
    
    const ParseStatistics& getStatistics() const { return m_statistics; }
    void resetStatistics() { m_statistics = ParseStatistics(); }
    
private:
    // Parser state
    struct ParserState {
        std::vector<Lexer::Token> tokens;
        size_t currentIndex = 0;
        std::stack<std::string> contextStack;
        std::unordered_map<std::string, uint8_t> packStateStack;
        size_t nestingDepth = 0;
        bool inStructDefinition = false;
        bool inUnionDefinition = false;
    };
    
    ParserOptions m_options;
    ParserState m_state;
    std::unique_ptr<AST::ASTBuilder> m_astBuilder;
    std::unique_ptr<Lexer::Tokenizer> m_tokenizer;
    std::unique_ptr<Lexer::Preprocessor> m_preprocessor;
    
    std::vector<ParseError> m_errors;
    std::vector<ParseError> m_warnings;
    ParseStatistics m_statistics;
    
    // Token navigation
    const Lexer::Token& currentToken() const;
    const Lexer::Token& peekToken(size_t offset = 1) const;
    bool hasMoreTokens() const;
    void advance();
    bool match(Lexer::TokenType expected);
    bool matchAndConsume(Lexer::TokenType expected);
    void skipTo(Lexer::TokenType tokenType);
    void skipToOneOf(const std::vector<Lexer::TokenType>& tokenTypes);
    
    // Parsing methods
    bool parseTopLevelDeclarations();
    std::unique_ptr<AST::StructDeclaration> parseStructDeclaration();
    std::unique_ptr<AST::UnionDeclaration> parseUnionDeclaration();
    std::unique_ptr<AST::TypedefDeclaration> parseTypedefDeclaration();
    
    // Field parsing
    std::vector<std::unique_ptr<AST::FieldDeclaration>> parseFieldDeclarations();
    std::unique_ptr<AST::FieldDeclaration> parseFieldDeclaration();
    std::unique_ptr<AST::BitfieldDeclaration> parseBitfieldDeclaration(
        const std::string& name, std::unique_ptr<AST::TypeNode> type);
    
    // Type parsing
    std::unique_ptr<AST::TypeNode> parseType();
    std::unique_ptr<AST::TypeNode> parseTypeSpecifier();
    std::unique_ptr<AST::PrimitiveType> parsePrimitiveType();
    std::unique_ptr<AST::NamedType> parseNamedType();
    std::unique_ptr<AST::ArrayType> parseArrayType(std::unique_ptr<AST::TypeNode> baseType);
    std::unique_ptr<AST::PointerType> parsePointerType(std::unique_ptr<AST::TypeNode> baseType);
    
    // Pragma and attribute parsing
    bool parsePragmaDirective();
    bool parseAttributeDirective();
    
    // Utility methods
    bool isTypeKeyword(Lexer::TokenType tokenType) const;
    bool isTypeSpecifier(const Lexer::Token& token) const;
    bool isStorageClassSpecifier(Lexer::TokenType tokenType) const;
    std::string parseIdentifier();
    size_t parseIntegerLiteral();
    
    // Context management
    void enterContext(const std::string& contextName);
    void exitContext();
    std::string getCurrentContext() const;
    
    // Error recovery
    void synchronize();
    void synchronizeToNextDeclaration();
    void recoverFromError(const std::string& message);
    
    // Validation
    bool validateStructName(const std::string& name) const;
    bool validateFieldName(const std::string& name, const std::string& structName) const;
    bool validateNestingDepth() const;
    bool validateFieldCount(size_t count) const;
    
    // Error reporting
    void addError(const std::string& message);
    void addError(const std::string& message, const Lexer::Token& token);
    void addError(const std::string& message, size_t line, size_t column, size_t position);
    void addWarning(const std::string& message);
    void addWarning(const std::string& message, const Lexer::Token& token);
    
    // Reserved keywords and identifiers
    static const std::unordered_set<std::string>& getReservedKeywords();
    static const std::unordered_set<std::string>& getPrimitiveTypeNames();
    bool isReservedKeyword(const std::string& identifier) const;
    bool isPrimitiveTypeName(const std::string& name) const;
};

} // namespace Parser
} // namespace Monitor