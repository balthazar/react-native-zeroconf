// The serial thread a dns_sd backend runs on: every call into the backend and every dns_sd callback is
// delivered on it, so the backend needs no locks. Apple: a dispatch queue. Android: the embedded
// mDNSResponder's thread.
#pragma once

#include <dns_sd.h>

#include <cstdint>
#include <functional>

namespace rnzeroconf {

class Executor {
 public:
  using Task = std::function<void()>;
  using TimerId = uint64_t;

  virtual ~Executor() = default;
  // Runs the task on the executor's thread, after the current one
  virtual void Post(Task task) = 0;
  // Runs the task after a delay, unless cancelled first. Ids are never 0
  virtual TimerId PostDelayed(double seconds, Task task) = 0;
  virtual void Cancel(TimerId timer) = 0;
  // Delivers the callbacks of a new DNSServiceRef on the executor's thread
  virtual void Attach(DNSServiceRef ref) = 0;
};

} // namespace rnzeroconf
