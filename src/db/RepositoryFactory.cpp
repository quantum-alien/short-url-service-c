#include "RepositoryFactory.hpp"

#include <stdexcept>

#ifdef USE_POSTGRES
#include "PostgresRepository.hpp"
#endif

#ifdef USE_SQLITE
#include "SqliteRepository.hpp"
#endif

namespace url_shortener::db {

std::unique_ptr<IRepository> createRepository(const config::Config& cfg) {
    switch (cfg.db_type) {
        case config::DbType::Postgres: {
#ifdef USE_POSTGRES
            PostgresConnectionOptions opts;
            opts.host = cfg.db_host;
            opts.port = cfg.db_port;
            opts.dbname = cfg.db_name;
            opts.user = cfg.db_user;
            opts.password = cfg.db_password;
            return std::make_unique<PostgresRepository>(opts);
#else
            throw std::runtime_error("Собрано без поддержки PostgreSQL (USE_POSTGRES=OFF)");
#endif
        }
        case config::DbType::Sqlite: {
#ifdef USE_SQLITE
            return std::make_unique<SqliteRepository>(cfg.sqlite_path);
#else
            throw std::runtime_error("Собрано без поддержки SQLite (USE_SQLITE=OFF)");
#endif
        }
    }
    throw std::runtime_error("Неизвестный тип БД");
}

}  // namespace url_shortener::db
