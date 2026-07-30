#pragma once

#include <cstdint>
#include <string>

namespace url_shortener::core {

// Генерирует короткие ключи вида "aZ3kQ9x" из произвольного URL.
//
// Алгоритм:
//   1. XXH64(original_url + nonce) -> 64-битный хэш (быстрый, низкая
//      коллизионность, не требует криптостойкости).
//   2. base62-кодирование хэша.
//   3. Обрезка/дополнение до нужной длины.
//
// nonce используется для повторной генерации при коллизии короткого
// ключа (см. UrlShortenerService), не меняя исходный алгоритм хэширования.
class KeyGenerator {
public:
    explicit KeyGenerator(unsigned key_length = 7);

    std::string generate(const std::string& original_url, uint32_t nonce = 0) const;

private:
    unsigned key_length_;
};

}  // namespace url_shortener::core
