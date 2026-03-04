#include "World/Level/ThreadPool.h"

namespace mc {

ThreadPool::ThreadPool(std::size_t threads) {
  if (threads == 0) {
    threads = 1;
  }
  workers_.reserve(threads);
  for (std::size_t i = 0; i < threads; ++i) {
    workers_.emplace_back([this]() { workerLoop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  cv_.notify_all();
  for (std::thread& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void ThreadPool::enqueue(std::function<void()> job) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return;
    }
    jobs_.push(std::move(job));
  }
  cv_.notify_one();
}

void ThreadPool::workerLoop() {
  for (;;) {
    std::function<void()> job;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return stopping_ || !jobs_.empty(); });
      if (stopping_ && jobs_.empty()) {
        return;
      }
      job = std::move(jobs_.front());
      jobs_.pop();
    }
    job();
  }
}

}  // namespace mc
