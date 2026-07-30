#pragma once

#include <cstdint>
#include <string>

namespace url_shortener::config {

enum class DbType { Postgres, Sqlite };

struct Config {
    // Сервер
    std::string server_host = "0.0.0.0";
    uint16_t server_port = 8080;
    unsigned thread_pool_size = 4;      // 4-8 воркер-потоков
    std::string base_url = "http://localhost:8080";

    // БД
    DbType db_type = DbType::Sqlite;
    std::string db_host = "localhost";
    uint16_t db_port = 5432;
    std::string db_name = "url_shortener";
    std::string db_user = "postgres";
    std::string db_password = "postgres";
    std::string sqlite_path = "url_shortener.db";

    // Логика генерации ключей
    unsigned short_key_length = 7;
    unsigned max_generation_retries = 5;

    // Загружает конфигурацию из переменных окружения, недостающие
    // параметры остаются значениями по умолчанию.
    static Config fromEnv();
};

}  // namespace url_shortener::config
