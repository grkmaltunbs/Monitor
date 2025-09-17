#include "preprocessor.h"
#include <algorithm>
#include <sstream>

namespace Monitor {
namespace Parser {
namespace Lexer {

Preprocessor::Preprocessor() {
    initializePlatformDefaults();
}

void Preprocessor::initializePlatformDefaults() {
    // Default to 8-byte alignment (typical for 64-bit systems)
    m_defaultPackValue = 8;

    // Push default pack state
    m_packStack.push(PackState(m_defaultPackValue));
}

std::vector<Token> Preprocessor::process(const std::vector<Token>& tokens) {
    clearErrors();
    std::vector<Token> processedTokens;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens[i];

        if (token.type == TokenType::PRAGMA) {
            // Collect pragma directive tokens
            std::vector<Token> directiveTokens;
            directiveTokens.push_back(token);

            // Read until end of line or end of tokens
            ++i;
            while (i < tokens.size() &&
                   tokens[i].type != TokenType::NEWLINE &&
                   tokens[i].type != TokenType::EOF_TOKEN) {
                directiveTokens.push_back(tokens[i]);
                ++i;
            }

            processDirective(directiveTokens);

            // Don't include pragma tokens in output
            continue;
        }

        processedTokens.push_back(token);
    }

    return processedTokens;
}

bool Preprocessor::processDirective(const std::vector<Token>& directiveTokens) {
    if (directiveTokens.empty()) {
        return false;
    }

    // First token should be PRAGMA
    if (directiveTokens[0].type != TokenType::PRAGMA) {
        return false;
    }

    // Look for "pack" keyword
    for (size_t i = 1; i < directiveTokens.size(); ++i) {
        if (directiveTokens[i].value == "pack") {
            // Parse pack directive arguments
            std::vector<std::string> args = parseArguments(directiveTokens, i + 1);
            return processPragmaPack(args, directiveTokens[0].line, directiveTokens[0].column);
        }
    }

    return true; // Unknown pragma, but not an error
}

std::vector<std::string> Preprocessor::parseArguments(const std::vector<Token>& tokens, size_t startIndex) {
    std::vector<std::string> args;

    for (size_t i = startIndex; i < tokens.size(); ++i) {
        const Token& token = tokens[i];

        if (token.type == TokenType::LEFT_PAREN ||
            token.type == TokenType::RIGHT_PAREN ||
            token.type == TokenType::COMMA) {
            continue; // Skip delimiters
        }

        if (token.type == TokenType::IDENTIFIER ||
            token.type == TokenType::INTEGER_LITERAL) {
            args.push_back(token.value);
        }
    }

    return args;
}

bool Preprocessor::processPragmaPack(const std::vector<std::string>& args, size_t line, size_t column) {
    if (args.empty()) {
        addError("Empty pack directive", line, column, "pack");
        return false;
    }

    const std::string& command = args[0];

    if (command == "push") {
        // #pragma pack(push[, identifier], value)
        if (args.size() >= 2) {
            try {
                uint8_t packValue = std::stoi(args.back());
                if (isValidPackValue(packValue)) {
                    std::string identifier = args.size() > 2 ? args[1] : "";
                    m_packStack.push(PackState(packValue, identifier));
                } else {
                    addError("Invalid pack value: " + args.back(), line, column, "pack");
                    return false;
                }
            } catch (const std::exception&) {
                addError("Invalid pack value: " + args.back(), line, column, "pack");
                return false;
            }
        } else {
            // Push current pack value
            if (!m_packStack.empty()) {
                m_packStack.push(m_packStack.top());
            }
        }
    } else if (command == "pop") {
        // #pragma pack(pop[, identifier])
        if (!m_packStack.empty()) {
            if (args.size() > 1) {
                // Pop until identifier matches
                std::string targetId = args[1];
                while (!m_packStack.empty() && m_packStack.top().identifier != targetId) {
                    m_packStack.pop();
                }
                if (!m_packStack.empty()) {
                    m_packStack.pop();
                }
            } else {
                m_packStack.pop();
            }
        }

        // Ensure we always have at least the default
        if (m_packStack.empty()) {
            m_packStack.push(PackState(m_defaultPackValue));
        }
    } else {
        // Direct pack value: #pragma pack(n)
        try {
            uint8_t packValue = std::stoi(command);
            if (isValidPackValue(packValue)) {
                if (!m_packStack.empty()) {
                    m_packStack.pop();
                }
                m_packStack.push(PackState(packValue));
            } else {
                addError("Invalid pack value: " + command, line, column, "pack");
                return false;
            }
        } catch (const std::exception&) {
            addError("Invalid pack value: " + command, line, column, "pack");
            return false;
        }
    }

    return true;
}

bool Preprocessor::processInclude(const std::vector<std::string>& args, size_t line, size_t column) {
    (void)args;
    (void)line;
    (void)column;
    // Basic include processing - just track that it was seen
    return true;
}

bool Preprocessor::processDefine(const std::vector<std::string>& args, size_t line, size_t column) {
    (void)line;
    (void)column;
    if (args.size() >= 2) {
        defineMacro(args[0], args[1]);
    }
    return true;
}

uint8_t Preprocessor::getCurrentPackValue() const {
    return m_packStack.empty() ? m_defaultPackValue : m_packStack.top().packValue;
}

bool Preprocessor::isPackActive() const {
    return !m_packStack.empty() && m_packStack.top().packValue != m_defaultPackValue;
}

void Preprocessor::defineMacro(const std::string& name, const std::string& value) {
    m_macros[name] = value;
}

void Preprocessor::undefineMacro(const std::string& name) {
    m_macros.erase(name);
}

bool Preprocessor::isMacroDefined(const std::string& name) const {
    return m_macros.find(name) != m_macros.end();
}

std::string Preprocessor::getMacroValue(const std::string& name) const {
    auto it = m_macros.find(name);
    return it != m_macros.end() ? it->second : "";
}

bool Preprocessor::isValidPackValue(uint8_t value) const {
    // Valid pack values are powers of 2: 1, 2, 4, 8, 16
    return value > 0 && (value & (value - 1)) == 0 && value <= 16;
}

void Preprocessor::addError(const std::string& message, size_t line, size_t column, const std::string& directive) {
    PreprocessorError error;
    error.message = message;
    error.line = line;
    error.column = column;
    error.directiveName = directive;
    m_errors.push_back(error);
}

void Preprocessor::reset() {
    // Clear pack stack and reset to default
    while (!m_packStack.empty()) {
        m_packStack.pop();
    }
    m_packStack.push(PackState(m_defaultPackValue));

    m_macros.clear();
    m_errors.clear();
    m_directives.clear();
}

Preprocessor::DirectiveType Preprocessor::identifyDirective(const std::string& directiveName) const {
    if (directiveName == "pack") {
        return DirectiveType::PRAGMA_PACK_SET;
    }
    if (directiveName == "include") {
        return DirectiveType::INCLUDE;
    }
    if (directiveName == "define") {
        return DirectiveType::DEFINE;
    }
    return DirectiveType::UNKNOWN;
}

} // namespace Lexer
} // namespace Parser
} // namespace Monitor