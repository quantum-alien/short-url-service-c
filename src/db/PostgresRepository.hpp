#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "IRepository.hpp"

// Предварительное объявление, чтобы не тащить <pqxx/pqxx> в заголовок
// и не заставлять всех потребителей заголовка линковаться с libpqxx.
namespace pqxx {
class connection;
}

namespace url_shortener::db {

struct PostgresConnectionOptions {
    std::string host;
    uint16_t port = 5432;
    std::string dbname;
    std::string user;
    std::string password;
};

// Реализация репозитория поверх PostgreSQL (libpqxx).
//
// Потокобезопасность: pqxx::connection не является потокобезопасным для
// параллельных запросов на одном соединении, поэтому доступ к соединению
// сериализуется мьютексом. При высокой конкурентности предпочтительнее
// пул соединений (см. README, раздел "Дальнейшие улучшения").
class PostgresRepository final : public IRepository {
public:
    explicit PostgresRepository(const PostgresConnectionOptions& options);
    ~PostgresRepository() override;

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
    std::unique_ptr<pqxx::connection> connection_;
    std::mutex mutex_;
};

}  // namespace url_shortener::db
