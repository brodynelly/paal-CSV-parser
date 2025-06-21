#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock lock(this->queue_mutex);
                    this->condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); });

                    if (this->stop && this->tasks.empty())
                        return;

                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }
        });
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock lock(queue_mutex);
        if (max_queue_size > 0) {
            condition.wait(lock, [this] {
                return tasks.size() < max_queue_size || stop;
            });
        }
        tasks.emplace(std::move(task));
    }
    condition.notify_one();
}

size_t ThreadPool::getQueueSize() {
    std::lock_guard lock(queue_mutex);
    return tasks.size();
}

void ThreadPool::setMaxQueueSize(size_t size) {
    std::lock_guard lock(queue_mutex);
    max_queue_size = size;
    condition.notify_all();
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker: workers)
        worker.join();
}
