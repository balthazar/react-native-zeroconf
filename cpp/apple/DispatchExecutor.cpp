#include "DispatchExecutor.h"

#include <utility>

namespace rnzeroconf {

namespace {

struct Delayed {
  std::shared_ptr<void> timers;
  Executor::TimerId id;
};

} // namespace

DispatchExecutor::DispatchExecutor(const char *label) : queue_(dispatch_queue_create(label, DISPATCH_QUEUE_SERIAL)) {}

DispatchExecutor::~DispatchExecutor() {
#if !OS_OBJECT_USE_OBJC
  dispatch_release(queue_);
#endif
}

void DispatchExecutor::Post(Task task) {
  dispatch_async_f(queue_, new Task(std::move(task)), [](void *context) {
    std::unique_ptr<Task> task(static_cast<Task *>(context));
    (*task)();
  });
}

Executor::TimerId DispatchExecutor::PostDelayed(double seconds, Task task) {
  TimerId id = ++nextTimer_;
  {
    std::lock_guard<std::mutex> lock(timers_->mutex);
    timers_->pending[id] = std::make_shared<Task>(std::move(task));
  }
  auto *delayed = new Delayed{timers_, id};
  dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(seconds * NSEC_PER_SEC)), queue_, delayed, [](void *context) {
    std::unique_ptr<Delayed> delayed(static_cast<Delayed *>(context));
    auto timers = std::static_pointer_cast<Timers>(delayed->timers);
    std::shared_ptr<Task> task;
    {
      std::lock_guard<std::mutex> lock(timers->mutex);
      auto found = timers->pending.find(delayed->id);
      if (found == timers->pending.end()) {
        // Cancelled
        return;
      }
      task = found->second;
      timers->pending.erase(found);
    }
    (*task)();
  });
  return id;
}

void DispatchExecutor::Cancel(TimerId timer) {
  std::lock_guard<std::mutex> lock(timers_->mutex);
  timers_->pending.erase(timer);
}

void DispatchExecutor::Attach(DNSServiceRef ref) {
  DNSServiceSetDispatchQueue(ref, queue_);
}

} // namespace rnzeroconf
