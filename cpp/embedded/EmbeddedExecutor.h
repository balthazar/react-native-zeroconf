// Executor for the mDNSResponder embedded in the library (Android DNSSD). The responder runs its event
// loop on a thread of its own and delivers dns_sd callbacks there; the backend runs on a second thread,
// each task holding the responder's core lock, which every call into it requires.
#pragma once

#include "../dnssd/Executor.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace rnzeroconf {

class EmbeddedExecutor : public Executor {
 public:
  // The responder is a process-wide singleton, started by the first use
  static std::shared_ptr<EmbeddedExecutor> Shared();
  ~EmbeddedExecutor() override;

  void Post(Task task) override;
  TimerId PostDelayed(double seconds, Task task) override;
  void Cancel(TimerId timer) override;
  // The responder delivers callbacks by itself
  void Attach(DNSServiceRef) override {}

  // The error of the responder's start, 0 when it runs
  int StartError() const;

 private:
  using Clock = std::chrono::steady_clock;
  struct Timer {
    Clock::time_point due;
    Task task;
  };

  EmbeddedExecutor();
  void RunResponder();
  void RunTasks();

  std::mutex mutex_;
  std::condition_variable changed_;
  std::deque<Task> tasks_;
  std::map<TimerId, Timer> timers_;
  std::atomic<TimerId> nextTimer_{0};
  bool started_ = false;
  int startError_ = 0;
  bool stopping_ = false;
  std::thread responder_;
  std::thread worker_;
};

} // namespace rnzeroconf
