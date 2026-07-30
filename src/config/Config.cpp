#include "Config.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace url_shortener::config {

namespace {

std::string getEnvOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? std::string(value) : fallback;
}

unsigned long getEnvUlOr(const char* name, unsigned long fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') return fallback;
    try {
        return std::stoul(value);
    } catch (const std::exception&) {
        return fallback;
    }
}

DbType parseDbType(const std::string& raw) {
    std::string lowered = raw;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    if (lowered == "postgres" || lowered == "postgresql") return DbType::Postgres;
    if (lowered == "sqlite" || lowered == "sqlite3") return DbType::Sqlite;
    throw std::invalid_argument("Неизвестный DB_TYPE: " + raw + " (ожидается postgres|sqlite)");
}

}  // namespace

Config Config::fromEnv() {
    Config cfg;

    cfg.server_host = getEnvOr("SERVER_HOST", cfg.server_host);
    cfg.server_port = static_cast<uint16_t>(getEnvUlOr("SERVER_PORT", cfg.server_port));
    cfg.thread_pool_size = static_cast<unsigned>(getEnvUlOr("THREAD_POOL_SIZE", cfg.thread_pool_size));
    cfg.thread_pool_size = std::clamp(cfg.thread_pool_size, 1u, 64u);
    cfg.base_url = getEnvOr("BASE_URL", cfg.base_url);

    cfg.db_type = parseDbType(getEnvOr("DB_TYPE", "sqlite"));
    cfg.db_host = getEnvOr("DB_HOST", cfg.db_host);
    cfg.db_port = static_cast<uint16_t>(getEnvUlOr("DB_PORT", cfg.db_port));
    cfg.db_name = getEnvOr("DB_NAME", cfg.db_name);
    cfg.db_user = getEnvOr("DB_USER", cfg.db_user);
    cfg.db_password = getEnvOr("DB_PASSWORD", cfg.db_password);
    cfg.sqlite_path = getEnvOr("SQLITE_PATH", cfg.sqlite_path);

    cfg.short_key_length = static_cast<unsigned>(getEnvUlOr("SHORT_KEY_LENGTH", cfg.short_key_length));
    cfg.max_generation_retries = static_cast<unsigned>(getEnvUlOr("MAX_GENERATION_RETRIES", cfg.max_generation_retries));

    return cfg;
}

}  // namespace url_shortener::config
