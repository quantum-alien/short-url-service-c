#pragma once

#include <cstdint>
#include <string>

namespace url_shortener::core {

// Кодирование/декодирование в алфавите [0-9a-zA-Z] (62 символа).
// Используется для превращения 64-битного хэша в компактный,
// URL-безопасный короткий ключ.
namespace base62 {

std::string encode(uint64_t value);
uint64_t decode(const std::string& encoded);

}  // namespace base62
}  // namespace url_shortener::core
