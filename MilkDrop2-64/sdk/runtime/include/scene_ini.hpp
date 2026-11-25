// SPDX-License-Identifier: MIT
#pragma once
#include "decimal_parse.hpp"
#include <charconv>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace suite {

class SceneIni {
    std::map<std::string, std::string, std::less<>> values_;

    static std::string_view trim(std::string_view text) {
        const auto first = text.find_first_not_of(" \t\r");
        if (first == std::string_view::npos) return {};
        return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
    }

public:
    std::string document() const {
        std::string section, result;
        for (const auto& [qualified, value] : values_) {
            const auto separator = qualified.rfind('.');
            if (separator == std::string::npos) throw std::runtime_error("Scene INI option is missing its section");
            const auto next = qualified.substr(0, separator);
            if (next != section) {
                if (!result.empty()) result += '\n';
                result += "[" + next + "]\n";
                section = next;
            }
            result += qualified.substr(separator + 1) + "=" + value + "\n";
        }
        return result;
    }
    const auto& values() const { return values_; }

    void apply(std::string_view text) {
        if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
        std::string section;
        std::map<std::string, std::string, std::less<>> parsed;
        for (unsigned line = 1; !text.empty(); ++line) {
            auto end = text.find('\n');
            const auto raw = text.substr(0, end);
            text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
            const auto entry = trim(raw.substr(0, raw.find_first_of(";#")));
            if (entry.empty()) continue;
            if (entry.starts_with('[') && entry.ends_with(']')) {
                section = trim(entry.substr(1, entry.size() - 2));
                if (!section.empty()) continue;
            }
            const auto equals = entry.find('=');
            if (equals != std::string_view::npos && !section.empty()) {
                const auto key = trim(entry.substr(0, equals));
                const auto value = trim(entry.substr(equals + 1));
                if (!key.empty() && key.find('.') == std::string_view::npos && !value.empty() &&
                    parsed.emplace(section + "." + std::string(key), value).second) continue;
            }
            throw std::runtime_error("Invalid or duplicate scene INI entry at line " + std::to_string(line));
        }
        for (auto& [key, value] : parsed) values_.insert_or_assign(key, std::move(value));
    }

    bool enabled(std::string_view key, bool fallback) const {
        const auto item = values_.find(key);
        if (item == values_.end()) return fallback;
        if (item->second == "true" || item->second == "1") return true;
        if (item->second == "false" || item->second == "0") return false;
        throw std::runtime_error("Expected true or false for scene INI option " + std::string(key));
    }

    template<class T>
    T number(std::string_view key, T fallback, T low, T high) const {
        const auto item = values_.find(key);
        if (item == values_.end()) return fallback;
        T result{};
        const auto& value = item->second;
        bool valid;
        if constexpr (std::is_integral_v<T>) {
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
            valid = parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
        } else {
            double decimal;
            valid = suite::finiteDecimal(value, decimal) && decimal >= low && decimal <= high;
            if (valid) result = static_cast<T>(decimal);
        }
        if (!valid || result < low || result > high)
            throw std::runtime_error("Invalid value for scene INI option " + std::string(key));
        return result;
    }
};

} // namespace suite
