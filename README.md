# URL Shortener — URL Shortening Microservice (C++ / Boost.Asio)

A C++20 microservice that shortens long URLs into compact short keys.

Implemented without web frameworks, using raw Boost.Asio with an explicit worker thread pool and support for either PostgreSQL or SQLite as the storage backend.

## Architecture

The project is divided into three independent layers that communicate through interfaces:

```text
src/
├── network/     # Networking layer: Boost.Asio TCP server, HTTP parser, thread pool, router
├── core/        # Business logic: key generation (xxHash + Base62), validation, service
├── db/          # Data layer: IRepository + PostgresRepository / SqliteRepository
├── config/      # Environment-based configuration
└── main.cpp     # Application composition and HTTP route registration
```

The key design principle is **dependency inversion**:

- `core::UrlShortenerService` depends only on the `db::IRepository` abstraction rather than a specific database implementation.
- `network::HttpServer` is completely independent of the business logic—the routes and handlers are provided externally (see `main.cpp`).

This allows you to:

- switch between PostgreSQL and SQLite using the `DB_TYPE` environment variable without recompiling the business logic;
- test `UrlShortenerService` using a mock implementation of `IRepository`;
- replace the networking layer (e.g., with Boost.Beast) without modifying the `core/` or `db/` modules.

### Networking Model and Concurrency

1. A single thread runs `boost::asio::io_context` with `acceptor_.async_accept`. Its only responsibility is accepting incoming TCP connections without blocking.
2. Every accepted connection (`std::shared_ptr<tcp::socket>`) is pushed as a task into `network::ThreadPool`, which uses a `std::queue<std::function<void()>>` protected by `std::mutex` and `std::condition_variable`.
3. The `ThreadPool` maintains 4–8 configurable worker threads. Each worker removes a task from the queue and **synchronously** reads the HTTP request, parses it, dispatches it through the router, sends the response, and closes the socket.

This approach is simpler than a fully asynchronous request pipeline while fully satisfying the project requirements: a worker thread pool processing a shared socket queue protected by `mutex` and `condition_variable`.

### Short Key Generation

`core::KeyGenerator` computes:

```
XXH64(original_url + nonce)
        ↓
   Base62 encoding
        ↓
Truncate to SHORT_KEY_LENGTH (default: 7)
```

If a generated short key already exists (`existsShortKey`), the service retries with a different `nonce` up to `MAX_GENERATION_RETRIES` times.

If the same URL is shortened again, the existing key is returned instead of generating a new one, making the operation **idempotent**.

### Database Schema

```text
users (id PK, username UNIQUE)
urls  (id PK, original_url, short_key UNIQUE, created_at, user_id FK -> users.id)
```

See:

- `sql/schema_postgres.sql`
- `sql/schema_sqlite.sql`

On startup, the application automatically executes `CREATE TABLE IF NOT EXISTS ...` through `IRepository::migrate`.

---

# HTTP API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | Health check |
| POST | `/api/v1/shorten` | Create a short URL |
| GET | `/{short_key}` | Redirect (302) to the original URL |

## POST /api/v1/shorten

Request:

```json
{
  "url": "https://example.com/very/long/path",
  "username": "alice"
}
```

`username` is optional. If provided, the shortened URL is associated with that user, who is automatically created on first use.

**201 Created**

```json
{
  "short_key": "aZ3kQ9x",
  "short_url": "http://localhost:8080/aZ3kQ9x",
  "original_url": "https://example.com/very/long/path",
  "already_existed": false
}
```

Example:

```bash
curl -X POST http://localhost:8080/api/v1/shorten \
     -H "Content-Type: application/json" \
     -d '{"url":"https://www.anthropic.com/"}'

curl -i http://localhost:8080/aZ3kQ9x
# -> HTTP/1.1 302 Found
# -> Location: https://www.anthropic.com/
```

---

# Building Locally

### Dependencies

- CMake ≥ 3.20
- C++20-compatible compiler
- Boost (`system`, `thread`)
- `libpqxx` + `libpq` (PostgreSQL)
- `libsqlite3` (SQLite)

`xxHash` and `SQLiteCpp` are fetched automatically using CMake `FetchContent`.

```bash
sudo apt-get install -y build-essential cmake \
    libboost-system-dev libboost-thread-dev \
    libpqxx-dev libpq-dev libsqlite3-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run locally with SQLite (no PostgreSQL required)
DB_TYPE=sqlite SQLITE_PATH=./url_shortener.db \
./build/src/url_shortener_server
```

To build with only one database backend (reducing dependencies):

```bash
cmake -S . -B build \
    -DUSE_POSTGRES=OFF \
    -DUSE_SQLITE=ON
```

---

# Running with Docker Compose

```bash
cp .env.example .env
docker compose up --build
```

This starts:

- `postgres:16-alpine`
- the application itself (built using a multi-stage Docker build: compilation in a builder image and deployment in a lightweight runtime image containing only the required shared libraries).

The service will be available at:

```
http://localhost:8080
```

---

# Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SERVER_HOST` | `0.0.0.0` | Listening address |
| `SERVER_PORT` | `8080` | Listening port |
| `THREAD_POOL_SIZE` | `4` | Number of worker threads (recommended: 4–8) |
| `BASE_URL` | `http://localhost:8080` | Base URL used when generating `short_url` |
| `DB_TYPE` | `sqlite` | `postgres` or `sqlite` |
| `DB_HOST` | `localhost` | PostgreSQL host |
| `DB_PORT` | `5432` | PostgreSQL port |
| `DB_NAME` | `url_shortener` | PostgreSQL database name |
| `DB_USER` | `postgres` | PostgreSQL username |
| `DB_PASSWORD` | `postgres` | PostgreSQL password |
| `SQLITE_PATH` | `url_shortener.db` | SQLite database file path |
| `SHORT_KEY_LENGTH` | `7` | Length of generated short keys |
| `MAX_GENERATION_RETRIES` | `5` | Maximum retries after key collisions |

---

# Future Improvements (Intentionally Out of Scope)

- PostgreSQL connection pooling (currently a single mutex-protected connection is sufficient for demonstration but limits throughput under high concurrency).
- A fully featured HTTP parser with keep-alive and chunked transfer encoding support (currently only `Connection: close` is supported). For production use, Boost.Beast is recommended.
- As a further production improvement, migrate to Boost.Beast with fully asynchronous per-connection processing.
- Replace the custom JSON parser with a mature library such as `nlohmann::json`.
- Add rate limiting and API key/JWT authentication for `/api/v1/shorten`.
- Add Prometheus metrics and structured logging.
- Add unit tests (GoogleTest) for `core::UrlShortenerService` using a mock `IRepository`.
