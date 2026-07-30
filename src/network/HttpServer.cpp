#include "HttpServer.hpp"

#include <iostream>
#include <sstream>
#include <vector>

#include "HttpParser.hpp"

namespace url_shortener::network {

using boost::asio::ip::tcp;

HttpServer::HttpServer(const std::string& host, uint16_t port, unsigned thread_pool_size)
    : acceptor_(io_context_), router_(), thread_pool_(thread_pool_size) {
    boost::asio::ip::address address = boost::asio::ip::make_address(host);
    tcp::endpoint endpoint(address, port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(boost::asio::socket_base::max_listen_connections);
}

HttpServer::~HttpServer() { stop(); }

void HttpServer::run() {
    running_ = true;
    doAccept();
    io_context_.run();
}

void HttpServer::stop() {
    if (!running_.exchange(false)) return;
    boost::system::error_code ec;
    acceptor_.close(ec);
    io_context_.stop();
    thread_pool_.stop();
}

void HttpServer::doAccept() {
    auto socket = std::make_shared<tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if (!ec) {
            // Обработка конкретного соединения передаётся в пул потоков —
            // acceptor-поток сразу возвращается к приёму следующих подключений,
            // не блокируясь на чтении/записи текущего.
            thread_pool_.submit([this, socket] { handleConnection(socket); });
        }
        if (running_) {
            doAccept();
        }
    });
}

void HttpServer::handleConnection(std::shared_ptr<tcp::socket> socket) {
    try {
        boost::asio::streambuf buffer;
        boost::system::error_code ec;

        // Читаем до конца заголовков ("\r\n\r\n"). read_until может
        // захватить в buffer часть тела запроса — это учитывается ниже.
        size_t header_bytes = boost::asio::read_until(*socket, buffer, "\r\n\r\n", ec);
        if (ec) {
            socket->close();
            return;
        }

        std::istream stream(&buffer);
        std::string head(header_bytes, '\0');
        stream.read(head.data(), static_cast<std::streamsize>(header_bytes));

        auto request_opt = HttpParser::parseHeaders(head);
        if (!request_opt) {
            HttpResponse bad_request;
            bad_request.setError(400, "Bad Request", "Не удалось разобрать запрос");
            boost::asio::write(*socket, boost::asio::buffer(bad_request.toString()), ec);
            socket->close();
            return;
        }

        HttpRequest request = std::move(*request_opt);
        const size_t content_length = HttpParser::contentLength(request);

        // Часть тела, уже оказавшаяся в streambuf после read_until.
        std::string body;
        body.reserve(content_length);
        if (buffer.size() > 0) {
            std::ostringstream oss;
            oss << &buffer;
            body += oss.str();
        }

        if (body.size() < content_length) {
            size_t remaining = content_length - body.size();
            std::vector<char> extra(remaining);
            size_t read_bytes = boost::asio::read(*socket, boost::asio::buffer(extra), ec);
            if (ec && ec != boost::asio::error::eof) {
                socket->close();
                return;
            }
            body.append(extra.data(), read_bytes);
        } else if (body.size() > content_length) {
            body.resize(content_length);
        }
        request.body = std::move(body);

        HttpResponse response = router_.route(request);

        std::string raw_response = response.toString();
        boost::asio::write(*socket, boost::asio::buffer(raw_response), ec);

        boost::system::error_code shutdown_ec;
        socket->shutdown(tcp::socket::shutdown_both, shutdown_ec);
        socket->close();
    } catch (const std::exception& ex) {
        std::cerr << "[HttpServer] Ошибка обработки соединения: " << ex.what() << std::endl;
        boost::system::error_code ec;
        socket->close(ec);
    }
}

}  // namespace url_shortener::network
