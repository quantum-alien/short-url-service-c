#pragma once

#include <boost/asio.hpp>
#include <atomic>
#include <memory>
#include <string>

#include "Router.hpp"
#include "ThreadPool.hpp"

namespace url_shortener::network {

// Сетевой слой: единственный поток крутит acceptor через
// boost::asio::io_context (async_accept), а фактическая обработка
// каждого принятого соединения (чтение запроса, роутинг, запись ответа)
// делегируется в ThreadPool, где N воркер-потоков разбирают очередь
// сокетов синхронно. Это соответствует классической модели
// "acceptor-поток + пул воркеров", которую просили в задании
// (очередь, защищённая mutex/condition_variable, находится внутри ThreadPool).
class HttpServer {
public:
    HttpServer(const std::string& host, uint16_t port, unsigned thread_pool_size);
    ~HttpServer();

    Router& router() { return router_; }

    // Блокирующий запуск: поднимает acceptor и крутит io_context в
    // текущем потоке до вызова stop().
    void run();

    void stop();

private:
    void doAccept();
    void handleConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Router router_;
    ThreadPool thread_pool_;
    std::atomic<bool> running_{false};
};

}  // namespace url_shortener::network
