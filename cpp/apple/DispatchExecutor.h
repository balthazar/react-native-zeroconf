// Executor on a serial dispatch queue: dns_sd delivers callbacks there with DNSServiceSetDispatchQueue
#pragma once

#include "../dnssd/Executor.h"

#include <dispatch/dispatch.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace rnzeroconf {

class DispatchExecutor : public Executor {
 public:
  explicit DispatchExecutor(const char *label);
  ~DispatchExecutor() override;

  void Post(Task task) override;
  TimerId PostDelayed(double seconds, Task task) override;
  void Cancel(TimerId timer) override;
  void Attach(DNSServiceRef ref) override;

 private:
  struct Timers {
    std::mutex mutex;
    std::map<TimerId, std::shared_ptr<Task>> pending;
  };

  dispatch_queue_t queue_;
  std::atomic<TimerId> nextTimer_{0};
  // Outlives the executor while timers are scheduled
  std::shared_ptr<Timers> timers_ = std::make_shared<Timers>();
};

} // namespace rnzeroconf
