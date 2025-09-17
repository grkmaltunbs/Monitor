#include "struct_parser.h"
#include <chrono>
#include <unordered_set>
#include <sstream>
#include <iostream>
#include <QDebug>

namespace Monitor {
namespace Parser {

// Constructor
StructParser::StructParser() {
    m_tokenizer = std::make_unique<Lexer::Tokenizer>();
    m_preprocessor = std::make_unique<Lexer::Preprocessor>();
    m_astBuilder = std::make_unique<AST::ASTBuilder>();
}

// Main parsing interface
StructParser::ParseResult StructParser::parse(const std::string& source) {
    auto startTime = std::chrono::high_resolution_clock::now();

    clearErrors();
    resetStatistics();

    ParseResult result;

    try {
        // Tokenize
        auto tokenizeStart = std::chrono::high_resolution_clock::now();
        std::vector<Lexer::Token> tokens = m_tokenizer->tokenize(source);
        auto tokenizeEnd = std::chrono::high_resolution_clock::now();
        m_statistics.tokenizationTime = std::chrono::duration_cast<std::chrono::milliseconds>(tokenizeEnd - tokenizeStart);
        m_statistics.tokensProcessed = tokens.size();

        if (m_tokenizer->hasErrors()) {
            for (const auto& error : m_tokenizer->getErrors()) {
                addError(error.message, error.line, error.column, error.position);
            }
            result.success = false;
            return result;
        }

        // Preprocess
        auto preprocessStart = std::chrono::high_resolution_clock::now();
        tokens = m_preprocessor->process(tokens);
        auto preprocessEnd = std::chrono::high_resolution_clock::now();
        m_statistics.preprocessingTime = std::chrono::duration_cast<std::chrono::milliseconds>(preprocessEnd - preprocessStart);

        if (m_preprocessor->hasErrors()) {
            for (const auto& error : m_preprocessor->getErrors()) {
                addError(error.message, error.line, error.column, 0);
            }
            result.success = false;
            return result;
        }

        // Parse
        auto parseStart = std::chrono::high_resolution_clock::now();
        result = parseFromTokens(tokens);
        auto parseEnd = std::chrono::high_resolution_clock::now();
        m_statistics.parsingTime = std::chrono::duration_cast<std::chrono::milliseconds>(parseEnd - parseStart);

    } catch (const std::exception& e) {
        addError("Parsing exception: " + std::string(e.what()));
        result.success = false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    m_statistics.totalTime = result.parseTime;

    return result;
}

StructParser::ParseResult StructParser::parseFromFile(const std::string& filePath) {
    std::vector<Lexer::Token> tokens = m_tokenizer->tokenizeFile(filePath);

    if (m_tokenizer->hasErrors()) {
        ParseResult result;
        for (const auto& error : m_tokenizer->getErrors()) {
            result.errors.push_back(error.message);
        }
        result.success = false;
        return result;
    }

    return parseFromTokens(tokens);
}

StructParser::ParseResult StructParser::parseFromTokens(const std::vector<Lexer::Token>& tokens) {
    m_state.tokens = tokens;
    m_state.currentIndex = 0;
    m_state.nestingDepth = 0;

    ParseResult result;

    try {
        if (parseTopLevelDeclarations()) {
            result.success = true;

            // Transfer parsed structures to result
            // Note: This is a simplified implementation
            // In a real implementation, we'd use the AST builder properly

            result.totalNodes = m_statistics.structsParsed + m_statistics.unionsParsed + m_statistics.typedefsParsed;
        } else {
            result.success = false;
        }

        // Copy errors and warnings
        for (const auto& error : m_errors) {
            result.errors.push_back(error.toString());
        }
        for (const auto& warning : m_warnings) {
            result.warnings.push_back(warning.toString());
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back("Exception during parsing: " + std::string(e.what()));
    }

    return result;
}

// Token navigation
const Lexer::Token& StructParser::currentToken() const {
    if (m_state.currentIndex >= m_state.tokens.size()) {
        static Lexer::Token eofToken(Lexer::TokenType::EOF_TOKEN, "", 0, 0, 0);
        return eofToken;
    }
    return m_state.tokens[m_state.currentIndex];
}

const Lexer::Token& StructParser::peekToken(size_t offset) const {
    size_t index = m_state.currentIndex + offset;
    if (index >= m_state.tokens.size()) {
        static Lexer::Token eofToken(Lexer::TokenType::EOF_TOKEN, "", 0, 0, 0);
        return eofToken;
    }
    return m_state.tokens[index];
}

bool StructParser::hasMoreTokens() const {
    return m_state.currentIndex < m_state.tokens.size() &&
           currentToken().type != Lexer::TokenType::EOF_TOKEN;
}

void StructParser::advance() {
    if (m_state.currentIndex < m_state.tokens.size()) {
        m_state.currentIndex++;
    }
}

bool StructParser::match(Lexer::TokenType expected) {
    return hasMoreTokens() && currentToken().type == expected;
}

bool StructParser::matchAndConsume(Lexer::TokenType expected) {
    if (match(expected)) {
        advance();
        return true;
    }
    return false;
}

void StructParser::skipTo(Lexer::TokenType tokenType) {
    while (hasMoreTokens() && currentToken().type != tokenType) {
        advance();
    }
}

void StructParser::skipToOneOf(const std::vector<Lexer::TokenType>& tokenTypes) {
    while (hasMoreTokens()) {
        Lexer::TokenType current = currentToken().type;
        for (auto type : tokenTypes) {
            if (current == type) {
                return;
            }
        }
        advance();
    }
}

// Top-level parsing
bool StructParser::parseTopLevelDeclarations() {
    bool hasValidDeclarations = false;

    while (hasMoreTokens()) {
        Lexer::TokenType current = currentToken().type;

        if (current == Lexer::TokenType::STRUCT) {
            auto structDecl = parseStructDeclaration();
            if (structDecl) {
                hasValidDeclarations = true;
                m_statistics.structsParsed++;
            }
        } else if (current == Lexer::TokenType::UNION) {
            auto unionDecl = parseUnionDeclaration();
            if (unionDecl) {
                hasValidDeclarations = true;
                m_statistics.unionsParsed++;
            }
        } else if (current == Lexer::TokenType::TYPEDEF) {
            auto typedefDecl = parseTypedefDeclaration();
            if (typedefDecl) {
                hasValidDeclarations = true;
                m_statistics.typedefsParsed++;
            }
        } else if (current == Lexer::TokenType::PRAGMA) {
            if (parsePragmaDirective()) {
                m_statistics.pragmasProcessed++;
            }
        } else {
            // Skip unknown tokens or try to recover
            advance();
        }
    }

    return hasValidDeclarations || !hasErrors();
}

std::unique_ptr<AST::StructDeclaration> StructParser::parseStructDeclaration() {
    if (!matchAndConsume(Lexer::TokenType::STRUCT)) {
        addError("Expected 'struct' keyword");
        return nullptr;
    }

    std::string structName;
    if (match(Lexer::TokenType::IDENTIFIER)) {
        structName = currentToken().value;
        advance();
    } else {
        addError("Expected struct name");
        return nullptr;
    }

    if (!validateStructName(structName)) {
        addError("Invalid struct name: " + structName);
        return nullptr;
    }

    if (!matchAndConsume(Lexer::TokenType::LEFT_BRACE)) {
        addError("Expected '{' after struct name");
        return nullptr;
    }

    enterContext("struct " + structName);
    m_state.inStructDefinition = true;
    m_state.nestingDepth++;

    auto structDecl = std::make_unique<AST::StructDeclaration>(structName);

    // Parse fields
    auto fields = parseFieldDeclarations();
    for (auto& field : fields) {
        if (field) {
            structDecl->addField(std::move(field));
            m_statistics.fieldsParsed++;
        }
    }

    if (!matchAndConsume(Lexer::TokenType::RIGHT_BRACE)) {
        addError("Expected '}' to close struct");
        exitContext();
        m_state.inStructDefinition = false;
        m_state.nestingDepth--;
        return nullptr;
    }

    // Optional semicolon
    matchAndConsume(Lexer::TokenType::SEMICOLON);

    exitContext();
    m_state.inStructDefinition = false;
    m_state.nestingDepth--;

    return structDecl;
}

std::unique_ptr<AST::UnionDeclaration> StructParser::parseUnionDeclaration() {
    if (!matchAndConsume(Lexer::TokenType::UNION)) {
        addError("Expected 'union' keyword");
        return nullptr;
    }

    std::string unionName;
    if (match(Lexer::TokenType::IDENTIFIER)) {
        unionName = currentToken().value;
        advance();
    } else {
        addError("Expected union name");
        return nullptr;
    }

    if (!matchAndConsume(Lexer::TokenType::LEFT_BRACE)) {
        addError("Expected '{' after union name");
        return nullptr;
    }

    enterContext("union " + unionName);
    m_state.inUnionDefinition = true;
    m_state.nestingDepth++;

    auto unionDecl = std::make_unique<AST::UnionDeclaration>(unionName);

    // Parse members
    auto fields = parseFieldDeclarations();
    for (auto& field : fields) {
        if (field) {
            unionDecl->addMember(std::move(field));
            m_statistics.fieldsParsed++;
        }
    }

    if (!matchAndConsume(Lexer::TokenType::RIGHT_BRACE)) {
        addError("Expected '}' to close union");
        exitContext();
        m_state.inUnionDefinition = false;
        m_state.nestingDepth--;
        return nullptr;
    }

    matchAndConsume(Lexer::TokenType::SEMICOLON);

    exitContext();
    m_state.inUnionDefinition = false;
    m_state.nestingDepth--;

    return unionDecl;
}

std::unique_ptr<AST::TypedefDeclaration> StructParser::parseTypedefDeclaration() {
    if (!matchAndConsume(Lexer::TokenType::TYPEDEF)) {
        addError("Expected 'typedef' keyword");
        return nullptr;
    }

    // Parse the underlying type
    auto underlyingType = parseType();
    if (!underlyingType) {
        addError("Expected type after 'typedef'");
        return nullptr;
    }

    // Parse the new type name
    std::string typeName = parseIdentifier();
    if (typeName.empty()) {
        addError("Expected typedef name");
        return nullptr;
    }

    matchAndConsume(Lexer::TokenType::SEMICOLON);

    return std::make_unique<AST::TypedefDeclaration>(typeName, std::move(underlyingType));
}

// Field parsing
std::vector<std::unique_ptr<AST::FieldDeclaration>> StructParser::parseFieldDeclarations() {
    std::vector<std::unique_ptr<AST::FieldDeclaration>> fields;

    while (hasMoreTokens() && !match(Lexer::TokenType::RIGHT_BRACE)) {
        auto field = parseFieldDeclaration();
        if (field) {
            fields.push_back(std::move(field));
        } else {
            // Try to recover
            synchronizeToNextDeclaration();
        }
    }

    return fields;
}

std::unique_ptr<AST::FieldDeclaration> StructParser::parseFieldDeclaration() {
    // Parse type
    auto type = parseType();
    if (!type) {
        addError("Expected type in field declaration");
        return nullptr;
    }

    // Parse field name
    std::string fieldName = parseIdentifier();
    if (fieldName.empty()) {
        addError("Expected field name");
        return nullptr;
    }

    // Check for bitfield
    if (match(Lexer::TokenType::COLON)) {
        advance(); // consume ':'

        size_t bitWidth = parseIntegerLiteral();
        if (bitWidth == 0) {
            addError("Expected bitfield width");
            return nullptr;
        }

        auto bitfield = std::make_unique<AST::BitfieldDeclaration>(fieldName, std::move(type), static_cast<uint32_t>(bitWidth));
        m_statistics.bitfieldsParsed++;

        matchAndConsume(Lexer::TokenType::SEMICOLON);
        return std::move(bitfield);
    }

    // Check for array
    if (match(Lexer::TokenType::LEFT_BRACKET)) {
        advance(); // consume '['

        size_t arraySize = parseIntegerLiteral();
        if (arraySize == 0) {
            addError("Expected array size");
            return nullptr;
        }

        if (!matchAndConsume(Lexer::TokenType::RIGHT_BRACKET)) {
            addError("Expected ']' after array size");
            return nullptr;
        }

        type = std::make_unique<AST::ArrayType>(std::move(type), arraySize);
    }

    matchAndConsume(Lexer::TokenType::SEMICOLON);

    return std::make_unique<AST::FieldDeclaration>(fieldName, std::move(type));
}

// Type parsing
std::unique_ptr<AST::TypeNode> StructParser::parseType() {
    return parseTypeSpecifier();
}

std::unique_ptr<AST::TypeNode> StructParser::parseTypeSpecifier() {
    // Handle primitive types
    if (isTypeKeyword(currentToken().type)) {
        return parsePrimitiveType();
    }

    // Handle named types (struct, union, typedef names)
    if (match(Lexer::TokenType::IDENTIFIER)) {
        return parseNamedType();
    }

    // Handle struct/union definitions
    if (match(Lexer::TokenType::STRUCT) || match(Lexer::TokenType::UNION)) {
        addError("Nested struct/union definitions not yet supported");
        return nullptr;
    }

    return nullptr;
}

std::unique_ptr<AST::PrimitiveType> StructParser::parsePrimitiveType() {
    using Kind = AST::PrimitiveType::Kind;

    Lexer::TokenType tokenType = currentToken().type;
    advance();

    Kind kind;
    switch (tokenType) {
        case Lexer::TokenType::VOID:
            kind = Kind::VOID;
            break;
        case Lexer::TokenType::CHAR:
            kind = Kind::CHAR;
            break;
        case Lexer::TokenType::SHORT:
            kind = Kind::SHORT;
            break;
        case Lexer::TokenType::INT:
            kind = Kind::INT;
            break;
        case Lexer::TokenType::LONG:
            kind = Kind::LONG;
            break;
        case Lexer::TokenType::FLOAT:
            kind = Kind::FLOAT;
            break;
        case Lexer::TokenType::DOUBLE:
            kind = Kind::DOUBLE;
            break;
        case Lexer::TokenType::UNSIGNED:
            // Handle "unsigned int", "unsigned char", etc.
            if (match(Lexer::TokenType::INT)) {
                advance();
                kind = Kind::UNSIGNED_INT;
            } else if (match(Lexer::TokenType::CHAR)) {
                advance();
                kind = Kind::UNSIGNED_CHAR;
            } else if (match(Lexer::TokenType::SHORT)) {
                advance();
                kind = Kind::UNSIGNED_SHORT;
            } else if (match(Lexer::TokenType::LONG)) {
                advance();
                kind = Kind::UNSIGNED_LONG;
            } else {
                kind = Kind::UNSIGNED_INT; // default to unsigned int
            }
            break;
        case Lexer::TokenType::SIGNED:
            // Handle "signed int", "signed char", etc.
            if (match(Lexer::TokenType::INT)) {
                advance();
                kind = Kind::INT;
            } else if (match(Lexer::TokenType::CHAR)) {
                advance();
                kind = Kind::SIGNED_CHAR;
            } else {
                kind = Kind::INT; // default to signed int
            }
            break;
        default:
            addError("Unknown primitive type");
            return nullptr;
    }

    return std::make_unique<AST::PrimitiveType>(kind);
}

std::unique_ptr<AST::NamedType> StructParser::parseNamedType() {
    std::string typeName = parseIdentifier();
    if (typeName.empty()) {
        addError("Expected type name");
        return nullptr;
    }

    return std::make_unique<AST::NamedType>(typeName);
}

// Pragma parsing
bool StructParser::parsePragmaDirective() {
    // This is handled by the preprocessor, so we just skip
    skipTo(Lexer::TokenType::NEWLINE);
    return true;
}

bool StructParser::parseAttributeDirective() {
    // Skip attribute directives for now
    skipTo(Lexer::TokenType::SEMICOLON);
    return true;
}

// Utility methods
bool StructParser::isTypeKeyword(Lexer::TokenType tokenType) const {
    switch (tokenType) {
        case Lexer::TokenType::VOID:
        case Lexer::TokenType::CHAR:
        case Lexer::TokenType::SHORT:
        case Lexer::TokenType::INT:
        case Lexer::TokenType::LONG:
        case Lexer::TokenType::FLOAT:
        case Lexer::TokenType::DOUBLE:
        case Lexer::TokenType::SIGNED:
        case Lexer::TokenType::UNSIGNED:
            return true;
        default:
            return false;
    }
}

std::string StructParser::parseIdentifier() {
    if (match(Lexer::TokenType::IDENTIFIER)) {
        std::string name = currentToken().value;
        advance();
        return name;
    }
    return "";
}

size_t StructParser::parseIntegerLiteral() {
    if (match(Lexer::TokenType::INTEGER_LITERAL)) {
        std::string value = currentToken().value;
        advance();
        try {
            return std::stoull(value);
        } catch (const std::exception&) {
            addError("Invalid integer literal: " + value);
            return 0;
        }
    }
    return 0;
}

// Context management
void StructParser::enterContext(const std::string& contextName) {
    m_state.contextStack.push(contextName);
}

void StructParser::exitContext() {
    if (!m_state.contextStack.empty()) {
        m_state.contextStack.pop();
    }
}

std::string StructParser::getCurrentContext() const {
    return m_state.contextStack.empty() ? "" : m_state.contextStack.top();
}

// Error recovery
void StructParser::synchronize() {
    skipToOneOf({
        Lexer::TokenType::STRUCT,
        Lexer::TokenType::UNION,
        Lexer::TokenType::TYPEDEF,
        Lexer::TokenType::SEMICOLON,
        Lexer::TokenType::RIGHT_BRACE
    });
}

void StructParser::synchronizeToNextDeclaration() {
    skipToOneOf({
        Lexer::TokenType::STRUCT,
        Lexer::TokenType::UNION,
        Lexer::TokenType::TYPEDEF,
        Lexer::TokenType::RIGHT_BRACE,
        Lexer::TokenType::EOF_TOKEN
    });
}

void StructParser::recoverFromError(const std::string& message) {
    addError(message);
    synchronize();
}

// Validation
bool StructParser::validateStructName(const std::string& name) const {
    return !name.empty() && !isReservedKeyword(name);
}

bool StructParser::validateFieldName(const std::string& name, const std::string& structName) const {
    Q_UNUSED(structName);
    return !name.empty() && !isReservedKeyword(name);
}

bool StructParser::validateNestingDepth() const {
    return m_state.nestingDepth <= m_options.maxNestingDepth;
}

bool StructParser::validateFieldCount(size_t count) const {
    return count <= m_options.maxFieldsPerStruct;
}

// Error reporting
void StructParser::addError(const std::string& message) {
    const auto& token = currentToken();
    addError(message, token.line, token.column, token.position);
}

void StructParser::addError(const std::string& message, const Lexer::Token& token) {
    addError(message, token.line, token.column, token.position);
}

void StructParser::addError(const std::string& message, size_t line, size_t column, size_t position) {
    std::string context = getCurrentContext();
    m_errors.emplace_back(message, line, column, position, context);
}

void StructParser::addWarning(const std::string& message) {
    const auto& token = currentToken();
    addWarning(message, token);
}

void StructParser::addWarning(const std::string& message, const Lexer::Token& token) {
    std::string context = getCurrentContext();
    m_warnings.emplace_back(message, token.line, token.column, token.position, context);
}

// Static data
const std::unordered_set<std::string>& StructParser::getReservedKeywords() {
    static const std::unordered_set<std::string> keywords = {
        "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if",
        "inline", "int", "long", "register", "restrict", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while", "_Bool", "_Complex", "_Imaginary"
    };
    return keywords;
}

const std::unordered_set<std::string>& StructParser::getPrimitiveTypeNames() {
    static const std::unordered_set<std::string> types = {
        "void", "char", "short", "int", "long", "float", "double",
        "signed", "unsigned", "_Bool"
    };
    return types;
}

bool StructParser::isReservedKeyword(const std::string& identifier) const {
    const auto& keywords = getReservedKeywords();
    return keywords.find(identifier) != keywords.end();
}

bool StructParser::isPrimitiveTypeName(const std::string& name) const {
    const auto& types = getPrimitiveTypeNames();
    return types.find(name) != types.end();
}

// ParseError implementation
std::string StructParser::ParseError::toString() const {
    std::stringstream ss;
    ss << message << " at line " << line << ", column " << column;
    if (!context.empty()) {
        ss << " (in " << context << ")";
    }
    return ss.str();
}

} // namespace Parser
} // namespace Monitor