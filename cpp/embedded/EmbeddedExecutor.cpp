#include "EmbeddedExecutor.h"

#include <utility>

// The embedded responder's entry points (mDNSPosix/PosixDaemon.c)
extern "C" {
int init();
int loop();
void stopLoop();
void lockCore();
void unlockCore();
}

namespace rnzeroconf {

// Started by the first use and kept for the life of the process: the responder can only be initialized
// once at a time, and backends release their last reference from the executor's own tasks
std::shared_ptr<EmbeddedExecutor> EmbeddedExecutor::Shared() {
  static std::shared_ptr<EmbeddedExecutor> executor(new EmbeddedExecutor());
  return executor;
}

EmbeddedExecutor::EmbeddedExecutor() {
  responder_ = std::thread([this] { RunResponder(); });
  worker_ = std::thread([this] { RunTasks(); });
}

EmbeddedExecutor::~EmbeddedExecutor() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  changed_.notify_all();
  // The last reference can go away in one of the executor's own tasks
  if (worker_.get_id() == std::this_thread::get_id()) {
    worker_.detach();
  } else {
    worker_.join();
  }
  stopLoop();
  responder_.join();
}

void EmbeddedExecutor::RunResponder() {
  int error = init();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = true;
    startError_ = error;
  }
  changed_.notify_all();
  if (error == 0) {
    loop();
  }
}

int EmbeddedExecutor::StartError() const {
  return startError_;
}

void EmbeddedExecutor::RunTasks() {
  std::unique_lock<std::mutex> lock(mutex_);
  // dns_sd calls need the responder initialized
  changed_.wait(lock, [this] { return started_ || stopping_; });
  while (true) {
    Task task;
    if (!tasks_.empty()) {
      task = std::move(tasks_.front());
      tasks_.pop_front();
    } else {
      auto next = timers_.end();
      for (auto it = timers_.begin(); it != timers_.end(); ++it) {
        if (next == timers_.end() || it->second.due < next->second.due) {
          next = it;
        }
      }
      if (stopping_) {
        return;
      }
      if (next == timers_.end()) {
        changed_.wait(lock);
        continue;
      }
      if (Clock::now() < next->second.due) {
        changed_.wait_until(lock, next->second.due);
        continue;
      }
      task = std::move(next->second.task);
      timers_.erase(next);
    }
    lock.unlock();
    lockCore();
    task();
    // What the task holds is released under the core lock, outside the queue's
    task = nullptr;
    unlockCore();
    lock.lock();
  }
}

void EmbeddedExecutor::Post(Task task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push_back(std::move(task));
  }
  changed_.notify_all();
}

Executor::TimerId EmbeddedExecutor::PostDelayed(double seconds, Task task) {
  TimerId id = ++nextTimer_;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    timers_[id] = Timer{Clock::now() + std::chrono::milliseconds(static_cast<int64_t>(seconds * 1000)), std::move(task)};
  }
  changed_.notify_all();
  return id;
}

void EmbeddedExecutor::Cancel(TimerId timer) {
  std::lock_guard<std::mutex> lock(mutex_);
  timers_.erase(timer);
}

} // namespace rnzeroconf
