#include "SqliteRepository.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

namespace url_shortener::db {

namespace {

UrlRecord rowToUrlRecord(SQLite::Statement& stmt) {
    UrlRecord rec;
    rec.id = stmt.getColumn(0).getInt64();
    rec.original_url = stmt.getColumn(1).getString();
    rec.short_key = stmt.getColumn(2).getString();
    rec.created_at = stmt.getColumn(3).getString();
    if (!stmt.getColumn(4).isNull()) {
        rec.user_id = stmt.getColumn(4).getInt64();
    }
    return rec;
}

}  // namespace

SqliteRepository::SqliteRepository(const std::string& db_path)
    : db_(std::make_unique<SQLite::Database>(
          db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)) {
    // WAL включает параллельное чтение во время записи — важно, так как
    // несколько worker-потоков могут обращаться к БД одновременно.
    db_->exec("PRAGMA journal_mode=WAL;");
    db_->exec("PRAGMA foreign_keys=ON;");
}

SqliteRepository::~SqliteRepository() = default;

void SqliteRepository::migrate() {
    std::lock_guard<std::mutex> lock(mutex_);
    db_->exec(R"SQL(
        CREATE TABLE IF NOT EXISTS users (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            username    TEXT NOT NULL UNIQUE
        )
    )SQL");

    db_->exec(R"SQL(
        CREATE TABLE IF NOT EXISTS urls (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            original_url    TEXT NOT NULL,
            short_key       TEXT NOT NULL UNIQUE,
            created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
            user_id         INTEGER NULL REFERENCES users(id) ON DELETE SET NULL
        )
    )SQL");

    db_->exec("CREATE INDEX IF NOT EXISTS idx_urls_short_key ON urls(short_key)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_urls_original_url ON urls(original_url)");
}

int64_t SqliteRepository::saveUrl(const std::string& original_url,
                                   const std::string& short_key,
                                   std::optional<int64_t> user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    SQLite::Statement stmt(*db_,
        "INSERT INTO urls (original_url, short_key, user_id) VALUES (?, ?, ?)");
    stmt.bind(1, original_url);
    stmt.bind(2, short_key);
    if (user_id.has_value()) {
        stmt.bind(3, static_cast<int64_t>(*user_id));
    } else {
        stmt.bind(3);  // NULL
    }
    stmt.exec();
    return db_->getLastInsertRowid();
}

std::optional<UrlRecord> SqliteRepository::findByShortKey(const std::string& short_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    SQLite::Statement stmt(*db_,
        "SELECT id, original_url, short_key, created_at, user_id "
        "FROM urls WHERE short_key = ?");
    stmt.bind(1, short_key);
    if (!stmt.executeStep()) return std::nullopt;
    return rowToUrlRecord(stmt);
}

std::optional<UrlRecord> SqliteRepository::findByOriginalUrl(const std::string& original_url) {
    std::lock_guard<std::mutex> lock(mutex_);
    SQLite::Statement stmt(*db_,
        "SELECT id, original_url, short_key, created_at, user_id "
        "FROM urls WHERE original_url = ? ORDER BY created_at DESC LIMIT 1");
    stmt.bind(1, original_url);
    if (!stmt.executeStep()) return std::nullopt;
    return rowToUrlRecord(stmt);
}

bool SqliteRepository::existsShortKey(const std::string& short_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    SQLite::Statement stmt(*db_, "SELECT 1 FROM urls WHERE short_key = ? LIMIT 1");
    stmt.bind(1, short_key);
    return stmt.executeStep();
}

std::optional<UserRecord> SqliteRepository::findUserByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    SQLite::Statement stmt(*db_, "SELECT id, username FROM users WHERE username = ?");
    stmt.bind(1, username);
    if (!stmt.executeStep()) return std::nullopt;
    UserRecord rec;
    rec.id = stmt.getColumn(0).getInt64();
    rec.username = stmt.getColumn(1).getString();
    return rec;
}

int64_t SqliteRepository::createUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    SQLite::Statement stmt(*db_, "INSERT INTO users (username) VALUES (?)");
    stmt.bind(1, username);
    stmt.exec();
    return db_->getLastInsertRowid();
}

}  // namespace url_shortener::db
