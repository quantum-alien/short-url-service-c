#include "KeyGenerator.hpp"

#include <xxhash.h>

#include <algorithm>
#include <chrono>

#include "Base62.hpp"

namespace url_shortener::core {

KeyGenerator::KeyGenerator(unsigned key_length) : key_length_(key_length) {}

std::string KeyGenerator::generate(const std::string& original_url, uint32_t nonce) const {
    // Соль из nonce + грубой временной метки снижает вероятность того,
    // что два разных пользователя, сокращающих один и тот же URL "одновременно",
    // получат идентичный хэш при повторной попытке из-за коллизии.
    std::string payload = original_url;
    payload += '#';
    payload += std::to_string(nonce);

    const uint64_t hash = XXH64(payload.data(), payload.size(), /*seed=*/0x9E3779B97F4A7C15ULL);

    std::string encoded = base62::encode(hash);

    if (encoded.size() >= key_length_) {
        // Берём последние key_length_ символов — они лучше "перемешаны"
        // из-за особенностей деления в base62::encode.
        return encoded.substr(encoded.size() - key_length_);
    }

    // На случай короткого base62-представления (маловероятно, но
    // возможно для маленьких хэшей) — дополняем нулями по алфавиту.
    return std::string(key_length_ - encoded.size(), '0') + encoded;
}

}  // namespace url_shortener::core
