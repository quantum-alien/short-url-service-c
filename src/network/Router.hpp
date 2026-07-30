#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "HttpMessage.hpp"

namespace url_shortener::network {

using Handler = std::function<HttpResponse(const HttpRequest&)>;

// Простейший роутер с двумя видами маршрутов:
//   - точное совпадение (метод + путь), например "GET /health";
//   - маршрут "с параметром" на один сегмент, например "GET /:key",
//     который матчится на любой одиночный сегмент, не совпавший ни с
//     одним точным маршрутом (используется для редиректа по короткому ключу).
class Router {
public:
    void add(const std::string& method, const std::string& path, Handler handler) {
        routes_.push_back({method, path, std::move(handler)});
    }

    // Регистрирует "catch-all" обработчик для одного сегмента пути,
    // например GET /{short_key}.
    void addSingleSegmentFallback(const std::string& method, Handler handler) {
        fallback_handlers_[method] = std::move(handler);
    }

    HttpResponse route(const HttpRequest& request) const {
        for (const auto& route : routes_) {
            if (route.method == request.method && route.path == request.path) {
                return route.handler(request);
            }
        }

        // Одиночный сегмент вида "/abc123" без вложенных '/'
        if (request.path.size() > 1 &&
            request.path.find('/', 1) == std::string::npos) {
            auto it = fallback_handlers_.find(request.method);
            if (it != fallback_handlers_.end()) {
                return it->second(request);
            }
        }

        HttpResponse response;
        response.setError(404, "Not Found", "Маршрут не найден");
        return response;
    }

private:
    struct Route {
        std::string method;
        std::string path;
        Handler handler;
    };

    std::vector<Route> routes_;
    std::map<std::string, Handler> fallback_handlers_;
};

}  // namespace url_shortener::network
