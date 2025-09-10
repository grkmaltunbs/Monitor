#include "struct_parser.h"

namespace Monitor {
namespace Parser {

// Stub implementation for StructParser::ParseError
std::string StructParser::ParseError::toString() const {
    return message + " at line " + std::to_string(line) + ", column " + std::to_string(column);
}

} // namespace Parser
} // namespace Monitor