#include <csignal>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>

#include "config/Config.hpp"
#include "core/UrlShortenerService.hpp"
#include "db/RepositoryFactory.hpp"
#include "network/HttpServer.hpp"

using namespace url_shortener;

namespace {

// Сырой указатель на сервер только для корректной остановки по
// SIGINT/SIGTERM; сервер живёт на стеке main() и владеет собой сам.
network::HttpServer* g_server = nullptr;

void handleSignal(int) {
    if (g_server) g_server->stop();
}

// Крайне лёгкий JSON-парсер под нужды одного эндпоинта ({"url": "...",
// "user_id": ...}). Полноценная библиотека (nlohmann::json) — очевидное
// улучшение на будущее (см. README), но добавлять целую зависимость ради
// одного поля не оправдано для этого учебного примера.
std::optional<std::string> extractJsonStringField(const std::string& json, const std::string& field) {
    std::string needle = "\"" + field + "\"";
    auto key_pos = json.find(needle);
    if (key_pos == std::string::npos) return std::nullopt;
    auto colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos) return std::nullopt;
    auto quote_start = json.find('"', colon_pos);
    if (quote_start == std::string::npos) return std::nullopt;
    auto quote_end = json.find('"', quote_start + 1);
    if (quote_end == std::string::npos) return std::nullopt;
    return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

}  // namespace

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    try {
        config::Config cfg = config::Config::fromEnv();

        std::cout << "=== url-shortener ===\n"
                  << "DB type       : " << (cfg.db_type == config::DbType::Postgres ? "postgres" : "sqlite") << "\n"
                  << "Server        : " << cfg.server_host << ":" << cfg.server_port << "\n"
                  << "Thread pool   : " << cfg.thread_pool_size << " workers\n"
                  << "Base URL      : " << cfg.base_url << "\n"
                  << "Key length    : " << cfg.short_key_length << "\n"
                  << std::endl;

        auto repository = db::createRepository(cfg);
        repository->migrate();

        auto shared_repo = std::shared_ptr<db::IRepository>(std::move(repository));
        core::UrlShortenerService service(shared_repo, cfg.base_url, cfg.short_key_length,
                                           cfg.max_generation_retries);

        network::HttpServer server(cfg.server_host, cfg.server_port, cfg.thread_pool_size);
        g_server = &server;

        auto& router = server.router();

        router.add("GET", "/health", [](const network::HttpRequest&) {
            network::HttpResponse res;
            res.setJson(R"({"status":"ok"})");
            return res;
        });

        // POST /api/v1/shorten  { "url": "https://...", "username": "optional" }
        router.add("POST", "/api/v1/shorten", [&service, &shared_repo](const network::HttpRequest& req) {
            network::HttpResponse res;

            auto url_opt = extractJsonStringField(req.body, "url");
            if (!url_opt || url_opt->empty()) {
                res.setError(400, "Bad Request", "Поле \\\"url\\\" обязательно");
                return res;
            }

            std::optional<int64_t> user_id;
            if (auto username = extractJsonStringField(req.body, "username")) {
                auto existing_user = shared_repo->findUserByUsername(*username);
                user_id = existing_user ? existing_user->id : shared_repo->createUser(*username);
            }

            try {
                core::ShortenResult result = service.shorten(*url_opt, user_id);
                std::ostringstream body;
                body << "{"
                     << "\"short_key\":\"" << jsonEscape(result.short_key) << "\","
                     << "\"short_url\":\"" << jsonEscape(result.short_url) << "\","
                     << "\"original_url\":\"" << jsonEscape(result.original_url) << "\","
                     << "\"already_existed\":" << (result.already_existed ? "true" : "false")
                     << "}";
                res.setJson(body.str(), result.already_existed ? 200 : 201,
                            result.already_existed ? "OK" : "Created");
            } catch (const std::invalid_argument& ex) {
                res.setError(400, "Bad Request", jsonEscape(ex.what()));
            } catch (const std::exception& ex) {
                res.setError(500, "Internal Server Error", jsonEscape(ex.what()));
            }
            return res;
        });

        // GET /api/v1/info/{key} — метаданные без редиректа (полезно для UI/аналитики)
        router.add("GET", "/api/v1/info", [](const network::HttpRequest&) {
            network::HttpResponse res;
            res.setError(400, "Bad Request", "Укажите ключ: /api/v1/info?key=...");
            return res;
        });

        // GET /{key} — собственно редирект на оригинальный URL
        router.addSingleSegmentFallback("GET", [&service](const network::HttpRequest& req) {
            network::HttpResponse res;
            std::string key = req.path.substr(1);  // убираем ведущий '/'
            auto record = service.resolve(key);
            if (!record) {
                res.setError(404, "Not Found", "Короткая ссылка не найдена");
                return res;
            }
            res.setRedirect(record->original_url);
            return res;
        });

        std::cout << "Сервер слушает на http://" << cfg.server_host << ":" << cfg.server_port
                  << std::endl;
        server.run();
        g_server = nullptr;
    } catch (const std::exception& ex) {
        std::cerr << "Фатальная ошибка: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
