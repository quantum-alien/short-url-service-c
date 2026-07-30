-- Схема для PostgreSQL.
-- Применяется автоматически при старте приложения (Repository::migrate),
-- но также используется docker-compose для инициализации контейнера БД.

CREATE TABLE IF NOT EXISTS users (
    id          BIGSERIAL PRIMARY KEY,
    username    VARCHAR(64) NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS urls (
    id              BIGSERIAL PRIMARY KEY,
    original_url    TEXT NOT NULL,
    short_key       VARCHAR(16) NOT NULL UNIQUE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    user_id         BIGINT NULL REFERENCES users(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_urls_short_key ON urls(short_key);
CREATE INDEX IF NOT EXISTS idx_urls_original_url ON urls(original_url);
