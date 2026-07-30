-- Схема для SQLite. Применяется автоматически при старте приложения
-- (Repository::migrate); файл приведён для справки/ручного создания БД.

CREATE TABLE IF NOT EXISTS users (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    username    TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS urls (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    original_url    TEXT NOT NULL,
    short_key       TEXT NOT NULL UNIQUE,
    created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    user_id         INTEGER NULL REFERENCES users(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_urls_short_key ON urls(short_key);
CREATE INDEX IF NOT EXISTS idx_urls_original_url ON urls(original_url);
