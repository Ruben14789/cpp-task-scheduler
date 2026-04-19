#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
// A thread pool that executes tasks from a shared queue
class ThreadPool {
public:
    // Create a thread pool with a fixed number
    explicit ThreadPool(std::size_t thread_count);

    // Destructor stops the pool
    ~ThreadPool();

    // Prevent copying (thread pools should not be duplicated)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    

    template <class Func, class... Args>
    std::future<std::invoke_result_t<Func, Args...>>
    enqueue(Func&& func, Args&&... args);


private:
    // Worker threads that continuously execute tasks
    std::vector<std::thread> workers_;

    // Task queue
    std::queue<std::function<void()>> tasks_;

    // Synchronization primitives
    std::mutex mutex_;
    std::condition_variable condition_;

    // Flag indicating whether the pool is stop
    bool stopping_ = false;
};

// Constructor
inline ThreadPool::ThreadPool(std::size_t thread_count) {
    //if invalid amount
    if (thread_count == 0) {

        throw std::invalid_argument("thread_count must be greater than zero");
    }

    workers_.reserve(thread_count);

    // Create worker threads
    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                {
                    // Lock queue and wait for work or shutdown signal has been received
                    std::unique_lock<std::mutex> lock(mutex_);
                    condition_.wait(lock, [this] {
                        return stopping_ || !tasks_.empty();
                    });

                    // Exit thread if shutting down and no tasks remain
                    if (stopping_ && tasks_.empty()) {
                        return;
                    }

                    // Take next task from queue
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                // Execute task outside lock
                task();
            }
        });
    }
}

// Destructor: signals shutdown
inline ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    // Wake up works to exit out
    condition_.notify_all();

    // Wait for all threads to finish execution
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

template <class Func, class... Args>
auto ThreadPool::enqueue(Func&& func, Args&&... args)
    -> std::future<typename std::invoke_result_t<Func, Args...>> {
    using ReturnType = typename std::invoke_result_t<Func, Args...>;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

    std::future<ReturnType> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Prevent adding tasks after shutdown has started
        if (stopping_) {
            throw std::runtime_error("cannot enqueue on a stopped ThreadPool");
        }

        tasks_.emplace([task] {
            (*task)();
        });
    }

    // Notify one worker that new work is available to execute
    condition_.notify_one();
    return result;
}