#include "StringUtils.h"
#include <algorithm>

namespace StringUtils {
    std::vector<std::string_view> Split(std::string_view input, std::string_view delimiter, bool keepEmpties) {
        size_t last = 0;
        size_t next = 0;
        std::vector<std::string_view> result;

        while ((next = input.find(delimiter, last)) != std::string::npos) {
            if (keepEmpties || next - last > 0) {
                result.push_back(input.substr(last, next - last));
            }
            last = next + delimiter.size();
        }

        auto lastEntry = input.substr(last);
        if (lastEntry.length() > 0) {
            result.push_back(lastEntry);
        }
        return result;
    }

    std::string ToUpper(const std::string& str) {
        std::string result = str;
        std::transform(str.cbegin(), str.cend(), result.begin(), [](const char& letter) { return (char)std::toupper(letter); });
        return result;
    }

    std::string ToUpper(std::string_view str) {
        return ToUpper(std::string(str));
    }

    std::string ToLower(const std::string& str) {
        std::string result = str;
        std::transform(str.cbegin(), str.cend(), result.begin(), [](const char& letter) { return (char)std::tolower(letter); });
        return result;
    }

    std::string ToLower(std::string_view str) {
        return ToLower(std::string(str));
    }
}