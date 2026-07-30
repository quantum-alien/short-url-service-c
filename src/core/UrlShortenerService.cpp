#include "UrlShortenerService.hpp"

#include <regex>
#include <stdexcept>

namespace url_shortener::core {

namespace {
// Достаточно простая, но практичная проверка http(s) URL — полноценный
// парсинг URI (RFC 3986) избыточен для задачи сокращения ссылок.
const std::regex kUrlPattern(
    R"(^https?://[a-zA-Z0-9.-]+(:\d+)?(/[^\s]*)?$)",
    std::regex::icase);
}  // namespace

UrlShortenerService::UrlShortenerService(std::shared_ptr<db::IRepository> repository,
                                          std::string base_url,
                                          unsigned key_length,
                                          unsigned max_retries)
    : repository_(std::move(repository)),
      key_generator_(key_length),
      base_url_(std::move(base_url)),
      max_retries_(max_retries) {}

bool UrlShortenerService::isValidUrl(const std::string& url) {
    if (url.empty() || url.size() > 2048) return false;
    return std::regex_match(url, kUrlPattern);
}

ShortenResult UrlShortenerService::shorten(const std::string& original_url,
                                            std::optional<int64_t> user_id) {
    if (!isValidUrl(original_url)) {
        throw std::invalid_argument("Некорректный URL: " + original_url);
    }

    // Идемпотентность: если этот URL уже сокращали — вернуть существующий
    // ключ вместо создания дубликата записи.
    if (auto existing = repository_->findByOriginalUrl(original_url)) {
        ShortenResult result;
        result.short_key = existing->short_key;
        result.short_url = base_url_ + "/" + existing->short_key;
        result.original_url = existing->original_url;
        result.already_existed = true;
        return result;
    }

    std::string key;
    bool found_free_key = false;
    for (unsigned attempt = 0; attempt < max_retries_; ++attempt) {
        key = key_generator_.generate(original_url, /*nonce=*/attempt);
        if (!repository_->existsShortKey(key)) {
            found_free_key = true;
            break;
        }
    }

    if (!found_free_key) {
        throw std::runtime_error(
            "Не удалось сгенерировать уникальный короткий ключ за " +
            std::to_string(max_retries_) + " попыток");
    }

    repository_->saveUrl(original_url, key, user_id);

    ShortenResult result;
    result.short_key = key;
    result.short_url = base_url_ + "/" + key;
    result.original_url = original_url;
    result.already_existed = false;
    return result;
}

std::optional<db::UrlRecord> UrlShortenerService::resolve(const std::string& short_key) {
    return repository_->findByShortKey(short_key);
}

}  // namespace url_shortener::core
