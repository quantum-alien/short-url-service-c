#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace url_shortener::db {

struct UrlRecord {
    int64_t id = 0;
    std::string original_url;
    std::string short_key;
    std::string created_at;               // ISO-8601, генерируется БД
    std::optional<int64_t> user_id;
};

struct UserRecord {
    int64_t id = 0;
    std::string username;
};

}  // namespace url_shortener::db
