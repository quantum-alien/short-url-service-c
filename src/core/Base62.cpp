#include "Base62.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace url_shortener::core::base62 {

namespace {
constexpr std::string_view kAlphabet =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr int kBase = 62;
}  // namespace

std::string encode(uint64_t value) {
    if (value == 0) return std::string(1, kAlphabet[0]);

    std::string result;
    while (value > 0) {
        result.push_back(kAlphabet[value % kBase]);
        value /= kBase;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

uint64_t decode(const std::string& encoded) {
    uint64_t value = 0;
    for (char c : encoded) {
        auto pos = kAlphabet.find(c);
        if (pos == std::string_view::npos) {
            throw std::invalid_argument("Недопустимый символ base62: " + std::string(1, c));
        }
        value = value * kBase + static_cast<uint64_t>(pos);
    }
    return value;
}

}  // namespace url_shortener::core::base62
