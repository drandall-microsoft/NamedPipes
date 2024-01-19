#pragma once
#include <string_view>
#include <ostream>
#include <charconv>
#include <vector>

namespace ArgParse {
    template<typename T>
    bool TryParse(std::string_view sv, T& outResult, std::ostream& outErrorMessage) {
        static_assert(std::_Always_false<T>, "TryParse not implemented for type");
    }

    template<>
    inline bool TryParse(std::string_view sv, float& outResult, std::ostream& outErrorMessage) {
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), outResult);
        auto success = (ec == std::errc{} && *ptr == '\0');
        if (!success)
        {
            outErrorMessage << "Could not convert '" << sv << "' into a float.";
        }
        return success;
    }

    template<>
    inline bool TryParse(std::string_view sv, bool& outResult, std::ostream& outErrorMessage) {
        //auto text = StringUtils::ToLower(sv);
        auto text = sv;
        if (text == "true" || text == "1" || text == "on" || text == "t") {
            outResult = true;
            return true;
        }
        else if (text == "false" || text == "0" || text == "off" || text == "f") {
            outResult = false;
            return true;
        }

        outErrorMessage << "Could not convert '" << sv << "' into a bool.";
        return false;
    }

    template<>
    inline bool TryParse(std::string_view sv, std::string_view& outResult, std::ostream& outErrorMessage) {
        outResult = sv;
        return true;
    }

    template<>
    inline bool TryParse(std::string_view sv, std::string& outResult, std::ostream& outErrorMessage) {
        outResult = std::string(sv);
        return true;
    }

    template<>
    inline bool TryParse(std::string_view sv, char& outResult, std::ostream& outErrorMessage) {
        if (sv.size() != 1) {
            outErrorMessage << "Expected a single character, but got \"" << sv << "\" (" << sv.size() << " characters)";
            return false;
        }
        outResult = sv[0];
        return true;
    }

    template<typename T>
    inline bool TryParse(std::string_view sv, std::vector<T>& outResult, std::ostream& outErrorMessage) {
        T val;
        if (!TryParse(sv, val, outErrorMessage)) {
            return false;
        }
        outResult.push_back(val);
        return true;
    }
}