#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>

class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    void enqueue(std::function<void()> task);
    size_t getQueueSize();
    void setMaxQueueSize(size_t size);
    size_t getMaxQueueSize() const { return max_queue_size; }
    ~ThreadPool();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    size_t max_queue_size{0};

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
