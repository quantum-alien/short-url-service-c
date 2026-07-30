#pragma once

#include <optional>
#include <string>

#include "HttpMessage.hpp"

namespace url_shortener::network {

// Минималистичный HTTP/1.1-парсер, достаточный для простого REST API
// (без keep-alive, chunked transfer-encoding, multipart и т.п.).
// Каждое соединение обслуживается синхронно одним worker-потоком из
// ThreadPool, поэтому парсинг не требует асинхронного стейт-машина.
class HttpParser {
public:
    // Разбирает заголовки из уже прочитанной строки запроса + заголовков
    // (до "\r\n\r\n"). Тело запроса устанавливается отдельно через setBody,
    // так как оно читается по Content-Length уже после парсинга заголовков.
    static std::optional<HttpRequest> parseHeaders(const std::string& raw_head);

    // Извлекает значение Content-Length из уже распарсенных заголовков,
    // либо 0, если заголовок отсутствует / некорректен.
    static size_t contentLength(const HttpRequest& request);

private:
    static void parseQueryString(const std::string& query, HttpRequest& request);
    static std::string urlDecode(const std::string& encoded);
};

}  // namespace url_shortener::network
