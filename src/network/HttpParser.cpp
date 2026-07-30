#include "HttpParser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace url_shortener::network {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trim(const std::string& s) {
    const char* whitespace = " \t\r\n";
    const auto begin = s.find_first_not_of(whitespace);
    if (begin == std::string::npos) return "";
    const auto end = s.find_last_not_of(whitespace);
    return s.substr(begin, end - begin + 1);
}

}  // namespace

std::string HttpParser::urlDecode(const std::string& encoded) {
    std::string result;
    result.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            std::string hex = encoded.substr(i + 1, 2);
            try {
                result += static_cast<char>(std::stoi(hex, nullptr, 16));
                i += 2;
            } catch (...) {
                result += encoded[i];
            }
        } else if (encoded[i] == '+') {
            result += ' ';
        } else {
            result += encoded[i];
        }
    }
    return result;
}

void HttpParser::parseQueryString(const std::string& query, HttpRequest& request) {
    std::istringstream stream(query);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        if (pair.empty()) continue;
        auto eq_pos = pair.find('=');
        if (eq_pos == std::string::npos) {
            request.query_params[urlDecode(pair)] = "";
        } else {
            std::string key = urlDecode(pair.substr(0, eq_pos));
            std::string value = urlDecode(pair.substr(eq_pos + 1));
            request.query_params[key] = value;
        }
    }
}

std::optional<HttpRequest> HttpParser::parseHeaders(const std::string& raw_head) {
    std::istringstream stream(raw_head);
    std::string request_line;
    if (!std::getline(stream, request_line)) return std::nullopt;
    request_line = trim(request_line);

    std::istringstream line_stream(request_line);
    HttpRequest request;
    std::string full_target;
    if (!(line_stream >> request.method >> full_target)) return std::nullopt;

    auto q_pos = full_target.find('?');
    if (q_pos == std::string::npos) {
        request.path = urlDecode(full_target);
    } else {
        request.path = urlDecode(full_target.substr(0, q_pos));
        parseQueryString(full_target.substr(q_pos + 1), request);
    }

    if (request.path.empty()) request.path = "/";

    std::string header_line;
    while (std::getline(stream, header_line)) {
        header_line = trim(header_line);
        if (header_line.empty()) continue;
        auto colon = header_line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = toLower(trim(header_line.substr(0, colon)));
        std::string value = trim(header_line.substr(colon + 1));
        request.headers[key] = value;
    }

    return request;
}

size_t HttpParser::contentLength(const HttpRequest& request) {
    auto it = request.headers.find("content-length");
    if (it == request.headers.end()) return 0;
    try {
        return static_cast<size_t>(std::stoul(it->second));
    } catch (...) {
        return 0;
    }
}

}  // namespace url_shortener::network
