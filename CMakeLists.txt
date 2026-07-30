cmake_minimum_required(VERSION 3.20)
project(url_shortener VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(USE_POSTGRES "Собирать с поддержкой PostgreSQL (libpqxx)" ON)
option(USE_SQLITE   "Собирать с поддержкой SQLite (SQLiteCpp)"   ON)
option(BUILD_TESTS  "Собирать unit-тесты"                        OFF)

find_package(Boost 1.75 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

include(FetchContent)

# ---------------------------------------------------------------------------
# xxHash — быстрый некриптографический хэш, используется для генерации
# коротких ключей (XXH64 -> base62)
# ---------------------------------------------------------------------------
FetchContent_Declare(
  xxhash
  GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
  GIT_TAG        v0.8.2
)
FetchContent_GetProperties(xxhash)
if(NOT xxhash_POPULATED)
  FetchContent_Populate(xxhash)
endif()
add_library(xxhash STATIC ${xxhash_SOURCE_DIR}/xxhash.c)
target_include_directories(xxhash PUBLIC ${xxhash_SOURCE_DIR})

# ---------------------------------------------------------------------------
# PostgreSQL (libpqxx) — опционально
# ---------------------------------------------------------------------------
if(USE_POSTGRES)
  find_package(libpqxx QUIET CONFIG)
  if(NOT libpqxx_FOUND)
    find_library(PQXX_LIB NAMES pqxx)
    find_library(PQ_LIB NAMES pq)
    if(NOT PQXX_LIB OR NOT PQ_LIB)
      message(FATAL_ERROR "libpqxx/libpq не найдены. Установите libpqxx-dev libpq-dev или отключите USE_POSTGRES.")
    endif()
  endif()
endif()

# ---------------------------------------------------------------------------
# SQLite (SQLiteCpp) — опционально, удобно для локальной разработки/тестов
# ---------------------------------------------------------------------------
if(USE_SQLITE)
  FetchContent_Declare(
    sqlitecpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG        3.3.1
  )
  set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
  set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
  set(SQLITECPP_RUN_DOXYGEN OFF CACHE BOOL "" FORCE)
  set(SQLITE_ENABLE_COLUMN_METADATA OFF CACHE BOOL "" FORCE)
  set(SQLITECPP_INTERNAL_SQLITE ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(sqlitecpp)
endif()

add_subdirectory(src)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

message(STATUS "url_shortener: USE_POSTGRES=${USE_POSTGRES} USE_SQLITE=${USE_SQLITE} BUILD_TESTS=${BUILD_TESTS}")
