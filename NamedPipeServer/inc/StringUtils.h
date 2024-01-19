#pragma once

#include <vector>
#include <string>
#include <string_view>

namespace StringUtils {
    std::vector<std::string_view> Split(std::string_view input, std::string_view delimiter, bool keepEmpties = false);

    std::string ToUpper(const std::string& str);
    std::string ToUpper(std::string_view str);

    std::string ToLower(const std::string& str);
    std::string ToLower(std::string_view str);

    constexpr void TrimEnd(std::string& val) {
        val.erase(std::find_if(val.rbegin(), val.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), val.end());
    }
}