#include "src/parser/parser/struct_parser.h"
#include <iostream>
#include <string>

using namespace Monitor::Parser;

int main() {
    StructParser parser;

    // Test with a simple structure
    std::string testCode = R"(
        struct SimpleStruct {
            int x;
            int y;
            float value;
        };
    )";

    std::cout << "Testing C++ structure parser..." << std::endl;
    std::cout << "Input code:" << std::endl;
    std::cout << testCode << std::endl;

    auto result = parser.parse(testCode);

    std::cout << "\nParser results:" << std::endl;
    std::cout << "Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "Structures parsed: " << result.structures.size() << std::endl;
    std::cout << "Parse time: " << result.parseTime.count() << "ms" << std::endl;

    if (!result.errors.empty()) {
        std::cout << "\nErrors:" << std::endl;
        for (const auto& error : result.errors) {
            std::cout << "  - " << error << std::endl;
        }
    }

    if (!result.warnings.empty()) {
        std::cout << "\nWarnings:" << std::endl;
        for (const auto& warning : result.warnings) {
            std::cout << "  - " << warning << std::endl;
        }
    }

    return result.success ? 0 : 1;
}