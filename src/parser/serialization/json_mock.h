#pragma once

#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#else
// Mock nlohmann::json when not available
namespace nlohmann {
    class json {
    public:
        json() = default;
        json(const std::string&) {}
        json& operator[](const std::string&) { return *this; }
        const json& operator[](const std::string&) const { return *this; }
        template<typename T> json& operator=(const T&) { return *this; }
        std::string dump(int = -1) const { return "{}"; }
    };
}
#endif