#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace url_shortener::network {

// Классический пул из N воркер-потоков, разбирающих задачи
// (std::function<void()>) из общей очереди, защищённой mutex +
// condition_variable. Именно сюда HttpServer складывает принятые
// соединения — каждая задача целиком обслуживает один HTTP-запрос
// (парсинг, роутинг, ответ, закрытие сокета).
class ThreadPool {
public:
    explicit ThreadPool(unsigned num_threads) {
        if (num_threads == 0) num_threads = 1;
        workers_.reserve(num_threads);
        for (unsigned i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool() { stop(); }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;  // пул уже останавливается — задачу отбрасываем
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
    }

    size_t size() const noexcept { return workers_.size(); }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            try {
                task();
            } catch (...) {
                // Одна упавшая задача (например, сбой в обработке запроса)
                // не должна убивать воркер-поток.
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

}  // namespace url_shortener::network
