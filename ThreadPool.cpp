#include "ThreadPool.h"

using namespace std;

ThreadPool::ThreadPool(size_t workerCount)
    : activeTasks_(0), stopping_(false) {

    if (workerCount == 0) {
        throw invalid_argument("ThreadPool needs at least one worker thread");
    }

    // Reserve first so the vector never reallocates while threads are running.
    workers_.reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::workerLoop() {
    while (true) {
        function<void()> job;

        {
            // Sleep until there is work or we are asked to stop.
            unique_lock<mutex> lock(mtx_);
            taskReady_.wait(lock, [this]() {
                return stopping_ || !tasks_.empty();
            });

            // Exit only when stopping AND every queued task has been run.
            if (stopping_ && tasks_.empty()) {
                return;
            }

            job = move(tasks_.front());
            tasks_.pop();
            ++activeTasks_;
        }

        // Run the job outside the lock so other workers are not blocked.
        job();

        {
            lock_guard<mutex> lock(mtx_);
            --activeTasks_;
            if (tasks_.empty() && activeTasks_ == 0) {
                idle_.notify_all();
            }
        }
    }
}

size_t ThreadPool::workerCount() const {
    return workers_.size();
}

size_t ThreadPool::pendingTasks() const {
    lock_guard<mutex> guard(mtx_);
    return tasks_.size();
}

size_t ThreadPool::activeTasks() const {
    lock_guard<mutex> guard(mtx_);
    return activeTasks_;
}

void ThreadPool::waitUntilIdle() {
    unique_lock<mutex> lock(mtx_);
    idle_.wait(lock, [this]() {
        return tasks_.empty() && activeTasks_ == 0;
    });
}

bool ThreadPool::isRunning() const {
    lock_guard<mutex> guard(mtx_);
    return !stopping_;
}

void ThreadPool::shutdown() {
    {
        lock_guard<mutex> guard(mtx_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }

    // Wake every sleeping worker so they can observe the stop flag.
    taskReady_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

string ThreadPool::describe() const {
    lock_guard<mutex> guard(mtx_);
    return "ThreadPool[workers=" + to_string(workers_.size())
         + ", queued=" + to_string(tasks_.size())
         + ", active=" + to_string(activeTasks_)
         + ", state=" + (stopping_ ? "stopped" : "running") + "]";
}
