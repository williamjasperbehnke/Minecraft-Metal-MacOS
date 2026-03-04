#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mc {

class ThreadPool {
public:
  explicit ThreadPool(std::size_t threads);
  ~ThreadPool();

  void enqueue(std::function<void()> job);
  std::size_t threadCount() const { return workers_.size(); }

private:
  void workerLoop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> jobs_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stopping_ = false;
};

}  // namespace mc
