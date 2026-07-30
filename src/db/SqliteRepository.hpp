#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "IRepository.hpp"

namespace SQLite {
class Database;
}

namespace url_shortener::db {

// Реализация репозитория поверх SQLite (SQLiteCpp). Используется в
// основном для локальной разработки/тестов и как облегчённая
// альтернатива PostgreSQL, не требующая отдельного сервера БД.
class SqliteRepository final : public IRepository {
public:
    explicit SqliteRepository(const std::string& db_path);
    ~SqliteRepository() override;

    void migrate() override;

    int64_t saveUrl(const std::string& original_url,
                     const std::string& short_key,
                     std::optional<int64_t> user_id) override;

    std::optional<UrlRecord> findByShortKey(const std::string& short_key) override;
    std::optional<UrlRecord> findByOriginalUrl(const std::string& original_url) override;
    bool existsShortKey(const std::string& short_key) override;

    std::optional<UserRecord> findUserByUsername(const std::string& username) override;
    int64_t createUser(const std::string& username) override;

private:
    std::unique_ptr<SQLite::Database> db_;
    std::mutex mutex_;
};

}  // namespace url_shortener::db
