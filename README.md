# URL Shortener — микросервис сокращения ссылок (C++ / Boost.Asio)

Микросервис на C++20, сокращающий длинные URL до коротких ключей.
Реализован без веб-фреймворков поверх «сырого» Boost.Asio, с явным
пулом worker-потоков и поддержкой PostgreSQL или SQLite в качестве
хранилища.

## Архитектура

Проект разделён на три независимых слоя, взаимодействующих через интерфейсы:

```
src/
├── network/     # Сетевой слой: Boost.Asio TCP-сервер, HTTP-парсер, пул потоков, роутер
├── core/        # Бизнес-логика: генерация ключей (xxHash + base62), валидация, сервис
├── db/          # Слой данных: IRepository + PostgresRepository / SqliteRepository
├── config/      # Конфигурация из переменных окружения
└── main.cpp     # Композиция слоёв и объявление HTTP-маршрутов
```

Ключевой принцип — **инверсия зависимостей**: `core::UrlShortenerService`
знает только про абстракцию `db::IRepository`, а не про конкретную СУБД;
`network::HttpServer` ничего не знает о бизнес-логике — маршруты и
обработчики передаются извне (см. `main.cpp`). Это позволяет:

- подменять PostgreSQL ⇄ SQLite флагом `DB_TYPE` без перекомпиляции логики;
- тестировать `UrlShortenerService` с фейковой реализацией `IRepository`;
- заменить сетевой слой (например, на Boost.Beast) без изменения `core/` и `db/`.

### Сетевая модель и конкурентность

1. Один поток крутит `boost::asio::io_context` с `acceptor_.async_accept` —
   он только принимает TCP-соединения и не блокируется на них.
2. Каждое принятое соединение (`std::shared_ptr<tcp::socket>`) кладётся
   как задача в `network::ThreadPool` — очередь `std::queue<std::function<void()>>`,
   защищённую `std::mutex` + `std::condition_variable`.
3. `ThreadPool` держит 4–8 (настраивается) `std::thread`-воркеров, каждый
   из которых извлекает задачу из очереди и **синхронно** читает запрос,
   парсит его, вызывает роутер, пишет ответ и закрывает сокет.

Такая модель проще классического async-конвейера, но полностью
покрывает требование задания: пул воркер-потоков, разбирающих очередь
сокетов под mutex/condition_variable.

### Генерация коротких ключей

`core::KeyGenerator`: `XXH64(original_url + nonce)` → base62-кодирование
→ обрезка до `SHORT_KEY_LENGTH` (по умолчанию 7) символов. При коллизии
короткого ключа (`existsShortKey`) сервис повторяет генерацию с новым
`nonce` до `MAX_GENERATION_RETRIES` раз. Повторное сокращение уже
известного URL возвращает существующий ключ (идемпотентность).

### Схема БД

```
users (id PK, username UNIQUE)
urls  (id PK, original_url, short_key UNIQUE, created_at, user_id FK -> users.id)
```

См. `sql/schema_postgres.sql` и `sql/schema_sqlite.sql`. На старте
приложение само выполняет `CREATE TABLE IF NOT EXISTS ...` (`IRepository::migrate`).

## HTTP API

| Метод | Путь                    | Описание                                  |
|-------|-------------------------|--------------------------------------------|
| GET   | `/health`                | Проверка живости сервиса                   |
| POST  | `/api/v1/shorten`        | Создать короткую ссылку                    |
| GET   | `/{short_key}`           | Редирект (302) на оригинальный URL         |

### POST /api/v1/shorten

Запрос:
```json
{ "url": "https://example.com/very/long/path", "username": "alice" }
```
(`username` опционален — если передан, ссылка привязывается к пользователю,
который создаётся при первом обращении.)

Ответ `201 Created`:
```json
{
  "short_key": "aZ3kQ9x",
  "short_url": "http://localhost:8080/aZ3kQ9x",
  "original_url": "https://example.com/very/long/path",
  "already_existed": false
}
```

Пример:
```bash
curl -X POST http://localhost:8080/api/v1/shorten \
     -H "Content-Type: application/json" \
     -d '{"url":"https://www.anthropic.com/"}'

curl -i http://localhost:8080/aZ3kQ9x   # -> 302 Location: https://www.anthropic.com/
```

## Сборка локально

Зависимости: CMake ≥ 3.20, компилятор с поддержкой C++20, Boost (system, thread),
libpqxx + libpq (для PostgreSQL), libsqlite3 (для SQLite). xxHash и SQLiteCpp
подтягиваются автоматически через `FetchContent`.

```bash
sudo apt-get install -y build-essential cmake libboost-system-dev libboost-thread-dev \
                         libpqxx-dev libpq-dev libsqlite3-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Локально с SQLite (без поднятия PostgreSQL):
DB_TYPE=sqlite SQLITE_PATH=./url_shortener.db ./build/src/url_shortener_server
```

Сборка только с одной СУБД (уменьшает зависимости):
```bash
cmake -S . -B build -DUSE_POSTGRES=OFF -DUSE_SQLITE=ON
```

## Запуск через Docker Compose

```bash
cp .env.example .env
docker compose up --build
```

Поднимет `postgres:16-alpine` и приложение (multi-stage сборка —
компиляция в тяжёлом образе `builder`, рантайм в минимальном образе
только с shared-библиотеками). Сервис будет доступен на `http://localhost:8080`.

## Переменные окружения

| Переменная               | По умолчанию              | Описание                                 |
|---------------------------|---------------------------|--------------------------------------------|
| `SERVER_HOST`              | `0.0.0.0`                 | Адрес прослушивания                        |
| `SERVER_PORT`              | `8080`                    | Порт                                       |
| `THREAD_POOL_SIZE`         | `4`                       | Число worker-потоков (рекомендуется 4–8)   |
| `BASE_URL`                 | `http://localhost:8080`   | Базовый URL для формирования short_url     |
| `DB_TYPE`                  | `sqlite`                  | `postgres` \| `sqlite`                     |
| `DB_HOST` / `DB_PORT`      | `localhost` / `5432`      | Хост/порт PostgreSQL                       |
| `DB_NAME` / `DB_USER` / `DB_PASSWORD` | `url_shortener` / `postgres` / `postgres` | Учётные данные PostgreSQL |
| `SQLITE_PATH`              | `url_shortener.db`        | Путь к файлу SQLite                        |
| `SHORT_KEY_LENGTH`         | `7`                       | Длина короткого ключа                      |
| `MAX_GENERATION_RETRIES`   | `5`                       | Число попыток при коллизии ключа           |

## Дальнейшие улучшения (сознательно вне рамок примера)

- Пул соединений к PostgreSQL (сейчас одно соединение под мьютексом —
  достаточно для демонстрации, но ограничивает throughput при большом
  числе воркеров).
- Полноценный HTTP-парсер с keep-alive/chunked encoding (сейчас `Connection: close`).
  Для production рекомендуется Boost.Beast.
  Дальнейший шаг для промышленного использования - переход на Beast с
  честной асинхронностью на уровне каждого соединения.
- Полноценная JSON-библиотека (nlohmann::json) вместо самодельного парсинга.
- Rate limiting и авторизация по API-ключу/JWT для `/api/v1/shorten`.
- Метрики (Prometheus) и структурированное логирование.
- Юнит-тесты (GoogleTest) для `core::UrlShortenerService` с mock `IRepository`.
