#include "PostgresRepository.hpp"

#include <pqxx/pqxx>

namespace url_shortener::db {

namespace {

std::string buildConnString(const PostgresConnectionOptions& o) {
    return "host=" + o.host +
           " port=" + std::to_string(o.port) +
           " dbname=" + o.dbname +
           " user=" + o.user +
           " password=" + o.password +
           " connect_timeout=10";
}

UrlRecord rowToUrlRecord(const pqxx::row& row) {
    UrlRecord rec;
    rec.id = row["id"].as<int64_t>();
    rec.original_url = row["original_url"].as<std::string>();
    rec.short_key = row["short_key"].as<std::string>();
    rec.created_at = row["created_at"].as<std::string>();
    if (!row["user_id"].is_null()) {
        rec.user_id = row["user_id"].as<int64_t>();
    }
    return rec;
}

}  // namespace

PostgresRepository::PostgresRepository(const PostgresConnectionOptions& options)
    : connection_(std::make_unique<pqxx::connection>(buildConnString(options))) {}

PostgresRepository::~PostgresRepository() = default;

void PostgresRepository::migrate() {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    txn.exec(R"SQL(
        CREATE TABLE IF NOT EXISTS users (
            id          BIGSERIAL PRIMARY KEY,
            username    VARCHAR(64) NOT NULL UNIQUE
        )
    )SQL");

    txn.exec(R"SQL(
        CREATE TABLE IF NOT EXISTS urls (
            id              BIGSERIAL PRIMARY KEY,
            original_url    TEXT NOT NULL,
            short_key       VARCHAR(16) NOT NULL UNIQUE,
            created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            user_id         BIGINT NULL REFERENCES users(id) ON DELETE SET NULL
        )
    )SQL");

    txn.exec("CREATE INDEX IF NOT EXISTS idx_urls_short_key ON urls(short_key)");
    txn.exec("CREATE INDEX IF NOT EXISTS idx_urls_original_url ON urls(original_url)");
    txn.commit();
}

int64_t PostgresRepository::saveUrl(const std::string& original_url,
                                     const std::string& short_key,
                                     std::optional<int64_t> user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    // std::optional биндится libpqxx как SQL NULL при отсутствии значения —
    // корректнее и переносимее, чем вручную формировать строковый литерал.
    pqxx::row row = txn.exec_params1(
        "INSERT INTO urls (original_url, short_key, user_id) "
        "VALUES ($1, $2, $3) RETURNING id",
        original_url, short_key, user_id);
    txn.commit();
    return row["id"].as<int64_t>();
}

std::optional<UrlRecord> PostgresRepository::findByShortKey(const std::string& short_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    pqxx::result res = txn.exec_params(
        "SELECT id, original_url, short_key, created_at, user_id "
        "FROM urls WHERE short_key = $1",
        short_key);
    txn.commit();
    if (res.empty()) return std::nullopt;
    return rowToUrlRecord(res[0]);
}

std::optional<UrlRecord> PostgresRepository::findByOriginalUrl(const std::string& original_url) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    pqxx::result res = txn.exec_params(
        "SELECT id, original_url, short_key, created_at, user_id "
        "FROM urls WHERE original_url = $1 ORDER BY created_at DESC LIMIT 1",
        original_url);
    txn.commit();
    if (res.empty()) return std::nullopt;
    return rowToUrlRecord(res[0]);
}

bool PostgresRepository::existsShortKey(const std::string& short_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    pqxx::row row = txn.exec_params1(
        "SELECT EXISTS(SELECT 1 FROM urls WHERE short_key = $1)", short_key);
    txn.commit();
    return row[0].as<bool>();
}

std::optional<UserRecord> PostgresRepository::findUserByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    pqxx::result res = txn.exec_params(
        "SELECT id, username FROM users WHERE username = $1", username);
    txn.commit();
    if (res.empty()) return std::nullopt;
    UserRecord rec;
    rec.id = res[0]["id"].as<int64_t>();
    rec.username = res[0]["username"].as<std::string>();
    return rec;
}

int64_t PostgresRepository::createUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    pqxx::row row = txn.exec_params1(
        "INSERT INTO users (username) VALUES ($1) RETURNING id", username);
    txn.commit();
    return row["id"].as<int64_t>();
}

}  // namespace url_shortener::db
