#pragma once

#include <memory>
#include <optional>
#include <string>

#include "KeyGenerator.hpp"
#include "db/IRepository.hpp"

namespace url_shortener::core {

struct ShortenResult {
    std::string short_key;
    std::string short_url;
    std::string original_url;
    bool already_existed = false;  // true, если URL уже был сокращён ранее
};

enum class ShortenError { InvalidUrl, GenerationExhausted };

// Центральная бизнес-логика сервиса. Не знает ничего о HTTP или о
// конкретной СУБД — работает через db::IRepository и core::KeyGenerator.
class UrlShortenerService {
public:
    UrlShortenerService(std::shared_ptr<db::IRepository> repository,
                         std::string base_url,
                         unsigned key_length,
                         unsigned max_retries);

    // Возвращает ShortenResult либо бросает std::invalid_argument
    // (некорректный URL) / std::runtime_error (не удалось подобрать
    // свободный ключ после max_retries попыток).
    ShortenResult shorten(const std::string& original_url, std::optional<int64_t> user_id);

    // Возвращает оригинальный URL по короткому ключу, если он существует.
    std::optional<db::UrlRecord> resolve(const std::string& short_key);

    static bool isValidUrl(const std::string& url);

private:
    std::shared_ptr<db::IRepository> repository_;
    KeyGenerator key_generator_;
    std::string base_url_;
    unsigned max_retries_;
};

}  // namespace url_shortener::core
