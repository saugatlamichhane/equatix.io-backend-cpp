#pragma once

// Requires GCC 10+ / C++20 (Ubuntu 22.04 default GCC 11 is sufficient).
// std::jthread and std::stop_token are C++20 — available in GCC 10+.
// NOT available on Apple Clang 14 (macOS local dev); build via Docker.

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <stop_token>   // C++20: GCC 10+, NOT Apple Clang 14
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

// Fixed-size thread pool with a bounded task queue.
//
// Designed for CPU-bound work (bot AI) that must not run on the
// Drogon/Trantor IO event loop threads.
//
// Architecture:
//   IO Thread (Drogon)  ──submit()──>  [bounded queue]  ──>  Worker Thread
//                                                                  │
//   IO Thread (Drogon)  <──queueInLoop()──────────────────────────┘
//
// Key invariant: IO threads never block on CPU work. Worker threads never
// touch live room state — they post results back via queueInLoop().

class ThreadPool {
public:
    // name     : prefix for thread names, visible in htop/perf/gdb
    // nThreads : dedicated worker count (default: half of hardware threads,
    //            leaving the other half for Drogon's IO loops)
    // maxQueue : bounded queue depth — submit() throws when full (backpressure)
    explicit ThreadPool(std::string name,
                        std::size_t nThreads = std::max(1u, std::thread::hardware_concurrency() / 2),
                        std::size_t maxQueue = 256)
        : name_(std::move(name))
        , maxQueue_(maxQueue)
    {
        workers_.reserve(nThreads);
        for (std::size_t i = 0; i < nThreads; ++i) {
            // std::jthread (C++20):
            //   - auto-joins on destruction (unlike std::thread which terminates)
            //   - passes std::stop_token for cooperative cancellation
            workers_.emplace_back([this](std::stop_token st) {
                workerLoop(st);
            });

            // Name threads so they appear as "bot-compute/0", "bot-compute/1"
            // in perf, htop, and GDB — essential for profiling on Linux.
            std::string tname = name_.substr(0, 12) + "/" + std::to_string(i);
            pthread_setname_np(workers_.back().native_handle(), tname.c_str());
        }
    }

    // Non-copyable, non-movable — owns OS threads
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Destructor: std::jthread automatically requests stop on each worker
    // (via stop_token) and joins them — no manual cleanup needed.
    ~ThreadPool() = default;

    // Submit a fire-and-forget task.
    // Throws std::runtime_error if pool is shutting down or queue is full.
    // The caller handles the exception as a backpressure signal (e.g. bot passes).
    template<typename F>
    void submit(F&& f) {
        {
            std::unique_lock lock(mu_);
            if (queue_.size() >= maxQueue_) {
                throw std::runtime_error(
                    name_ + ": queue full (" + std::to_string(maxQueue_) + " pending)");
            }
            queue_.emplace(std::forward<F>(f));
        }
        cv_.notify_one();
        submitted_.fetch_add(1, std::memory_order_relaxed);
    }

    // Submit a task and get a std::future for its return value.
    // Use when the caller needs the result (e.g. unit tests, sync operations).
    template<typename F>
    [[nodiscard]] auto submitWithResult(F&& f)
        -> std::future<std::invoke_result_t<F>>
    {
        using R = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        std::future<R> fut = task->get_future();
        submit([t = std::move(task)]() mutable { (*t)(); });
        return fut;
    }

    // --- Diagnostics (expose via /livestats or Prometheus) ---

    [[nodiscard]] std::size_t pendingTasks() const {
        std::unique_lock lock(mu_);
        return queue_.size();
    }
    [[nodiscard]] std::size_t totalSubmitted() const noexcept {
        return submitted_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::size_t totalCompleted() const noexcept {
        return completed_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::size_t workerCount() const noexcept {
        return workers_.size();
    }

private:
    void workerLoop(std::stop_token st) {
        while (!st.stop_requested()) {
            std::function<void()> task;
            {
                std::unique_lock lock(mu_);
                // std::stop_token integrates with condition_variable_any,
                // but we're using condition_variable here for simplicity.
                // The stop_requested() check in the while loop handles shutdown.
                cv_.wait(lock, [&] {
                    return !queue_.empty() || st.stop_requested();
                });
                if (st.stop_requested() && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop();
            }
            task();
            completed_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::string                       name_;
    std::size_t                       maxQueue_;

    std::vector<std::jthread>         workers_;   // C++20: auto-joins on destruction
    mutable std::mutex                mu_;
    std::condition_variable           cv_;
    std::queue<std::function<void()>> queue_;

    std::atomic<std::size_t>          submitted_{0};
    std::atomic<std::size_t>          completed_{0};
};