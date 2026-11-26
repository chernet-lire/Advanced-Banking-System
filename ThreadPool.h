#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace std;

// A fixed-size pool of worker threads.
//
// Work is placed into a shared queue. Whichever worker is free takes the next
// task. enqueue() returns a std::future so the caller can wait for the result
// later, or simply discard the future if it does not need one.
//
// Lifetime rules:
//   * Destroying the pool finishes every task already queued, then joins threads.
//   * Once shutdown() has been called, enqueue() throws std::runtime_error.
class ThreadPool {
public:
    // Starts workerCount threads. Throws std::invalid_argument if zero.
    explicit ThreadPool(size_t workerCount);

    // Stops accepting work, finishes queued tasks, and joins all threads.
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submits any callable with arguments. The future holds the callable's
    // return value, or rethrows the exception it raised.
    template <typename Func, typename... Args>
    auto enqueue(Func&& func, Args&&... args)
        -> future<invoke_result_t<Func, Args...>>;

    // Number of worker threads owned by the pool. Fixed for its lifetime.
    size_t workerCount() const;

    // Number of tasks waiting in the queue. Running tasks are not counted.
    size_t pendingTasks() const;

    // Number of tasks currently executing on a worker thread.
    size_t activeTasks() const;

    // Blocks the caller until the queue is empty and no task is running.
    void waitUntilIdle();

    // True until shutdown() is called.
    bool isRunning() const;

    // Stops the pool. Safe to call more than once.
    void shutdown();

    // A one-line human-readable status string for logging.
    string describe() const;

private:
    // The loop each worker thread runs until the pool stops.
    void workerLoop();

    vector<thread> workers_;
    queue<function<void()>> tasks_;
    mutable mutex mtx_;
    condition_variable taskReady_;
    condition_variable idle_;
    size_t activeTasks_;
    bool stopping_;
};

// ---- Template implementation --------------------------------------------

template <typename Func, typename... Args>
auto ThreadPool::enqueue(Func&& func, Args&&... args)
    -> future<invoke_result_t<Func, Args...>> {

    using ReturnType = invoke_result_t<Func, Args...>;

    // packaged_task gives us a future for any callable. It is held in a
    // shared_ptr because std::function requires copyable callables.
    auto task = make_shared<packaged_task<ReturnType()>>(
        bind(forward<Func>(func), forward<Args>(args)...));

    future<ReturnType> result = task->get_future();

    {
        lock_guard<mutex> guard(mtx_);
        if (stopping_) {
            throw runtime_error("ThreadPool is shutting down; cannot enqueue");
        }
        tasks_.emplace([task]() { (*task)(); });
    }

    // Wake one sleeping worker. It is safe to do this outside the lock.
    taskReady_.notify_one();
    return result;
}

#endif // THREADPOOL_H
