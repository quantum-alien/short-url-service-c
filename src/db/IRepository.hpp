#pragma once

#include <optional>
#include <string>

#include "Models.hpp"

namespace url_shortener::db {

// Единый интерфейс слоя данных. Логика (core/) зависит только от этой
// абстракции, а не от конкретной СУБД — это позволяет подменять
// PostgresRepository <-> SqliteRepository без изменения бизнес-логики
// (принцип инверсии зависимостей).
class IRepository {
public:
    virtual ~IRepository() = default;

    // Создаёт таблицы, если их ещё нет (идемпотентно).
    virtual void migrate() = 0;

    // ---- urls -------------------------------------------------------
    virtual int64_t saveUrl(const std::string& original_url,
                             const std::string& short_key,
                             std::optional<int64_t> user_id) = 0;

    virtual std::optional<UrlRecord> findByShortKey(const std::string& short_key) = 0;
    virtual std::optional<UrlRecord> findByOriginalUrl(const std::string& original_url) = 0;
    virtual bool existsShortKey(const std::string& short_key) = 0;

    // ---- users --------------------------------------------------------
    virtual std::optional<UserRecord> findUserByUsername(const std::string& username) = 0;
    virtual int64_t createUser(const std::string& username) = 0;
};

}  // namespace url_shortener::db
