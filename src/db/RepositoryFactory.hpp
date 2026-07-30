#pragma once

#include <memory>

#include "IRepository.hpp"
#include "config/Config.hpp"

namespace url_shortener::db {

// Создаёт конкретную реализацию IRepository в зависимости от Config::db_type.
// Это единственное место в проекте, которое "знает" про обе СУБД —
// остальной код работает исключительно через интерфейс IRepository.
std::unique_ptr<IRepository> createRepository(const config::Config& cfg);

}  // namespace url_shortener::db
