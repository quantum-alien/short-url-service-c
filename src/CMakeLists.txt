add_library(url_shortener_core STATIC
  config/Config.cpp
  core/Base62.cpp
  core/KeyGenerator.cpp
  core/UrlShortenerService.cpp
  db/RepositoryFactory.cpp
  network/HttpParser.cpp
  network/HttpServer.cpp
)

target_include_directories(url_shortener_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(url_shortener_core PUBLIC
  Boost::system
  Boost::thread
  Threads::Threads
  xxhash
)

target_compile_options(url_shortener_core PRIVATE -Wall -Wextra -Wpedantic)

if(USE_POSTGRES)
  target_sources(url_shortener_core PRIVATE db/PostgresRepository.cpp)
  target_compile_definitions(url_shortener_core PUBLIC USE_POSTGRES)
  if(libpqxx_FOUND)
    target_link_libraries(url_shortener_core PUBLIC libpqxx::pqxx)
  else()
    target_link_libraries(url_shortener_core PUBLIC ${PQXX_LIB} ${PQ_LIB})
  endif()
endif()

if(USE_SQLITE)
  target_sources(url_shortener_core PRIVATE db/SqliteRepository.cpp)
  target_compile_definitions(url_shortener_core PUBLIC USE_SQLITE)
  target_link_libraries(url_shortener_core PUBLIC SQLiteCpp sqlite3)
endif()

add_executable(url_shortener_server main.cpp)
target_link_libraries(url_shortener_server PRIVATE url_shortener_core)

install(TARGETS url_shortener_server RUNTIME DESTINATION bin)
