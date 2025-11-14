#ifndef POOL_H
#define POOL_H
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>
#include <vector>

class thread_pool {
private:
    const unsigned int max_threads;
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::condition_variable cv;
    std::mutex mtx;
    std::atomic<bool> running_ = {false};
public:
    explicit thread_pool(const unsigned int max_threads): max_threads(max_threads), running_(true) {
        threads.reserve(max_threads);
        for (int i = 0; i < max_threads; i++) {
            threads.emplace_back([&]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [&]() { return !running_.load() || !tasks.empty(); });
                        if (tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    void stop() {
        if (!running_.load()) {
            return;
        }
        bool expected = true;
        while (running_.compare_exchange_strong(expected, false)) {}
        for (auto &t : threads) {
            t.join();
        }
    }

    template<typename F>
    void submit(F&& f) {
        {
            std::unique_lock lock(mtx);
            tasks.emplace(std::forward<F>(f));
        }
        cv.notify_one();
    }

    ~thread_pool() {
        stop();
    }

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
};

#endif