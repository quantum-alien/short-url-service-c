#pragma once

#include <map>
#include <sstream>
#include <string>

namespace url_shortener::network {

struct HttpRequest {
    std::string method;                    // GET, POST, ...
    std::string path;                      // без query string, например "/api/v1/shorten"
    std::map<std::string, std::string> query_params;
    std::map<std::string, std::string> headers;  // ключи в нижнем регистре
    std::string body;

    std::string header(const std::string& lower_name, const std::string& fallback = "") const {
        auto it = headers.find(lower_name);
        return it != headers.end() ? it->second : fallback;
    }
};

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::map<std::string, std::string> headers;
    std::string body;

    HttpResponse& setJson(const std::string& json_body, int code = 200,
                           const std::string& text = "OK") {
        status_code = code;
        status_text = text;
        headers["Content-Type"] = "application/json; charset=utf-8";
        body = json_body;
        return *this;
    }

    HttpResponse& setRedirect(const std::string& location, int code = 302) {
        status_code = code;
        status_text = "Found";
        headers["Location"] = location;
        headers["Content-Type"] = "text/plain; charset=utf-8";
        body = "Redirecting to " + location;
        return *this;
    }

    HttpResponse& setError(int code, const std::string& text, const std::string& message) {
        status_code = code;
        status_text = text;
        headers["Content-Type"] = "application/json; charset=utf-8";
        std::ostringstream oss;
        oss << R"({"error":")" << message << R"("})";
        body = oss.str();
        return *this;
    }

    // Сериализация в "сырой" HTTP/1.1 ответ, готовый к записи в сокет.
    std::string toString() const {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Connection: close\r\n";
        for (const auto& [key, value] : headers) {
            oss << key << ": " << value << "\r\n";
        }
        oss << "\r\n";
        oss << body;
        return oss.str();
    }
};

}  // namespace url_shortener::network
