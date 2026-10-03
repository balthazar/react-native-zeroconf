#include "ZeroconfModule.h"

#include <utility>

namespace facebook::react {

namespace {

rnzeroconf::TxtPairs ToTxt(const std::vector<ZeroconfTxtEntry> &entries) {
  rnzeroconf::TxtPairs txt;
  for (const auto &entry : entries) {
    txt.emplace_back(entry.key, entry.value);
  }
  return txt;
}

ZeroconfService ToJs(const rnzeroconf::Service &service) {
  std::vector<ZeroconfTxtEntry> txt;
  for (const auto &[key, value] : service.txt) {
    txt.push_back(ZeroconfTxtEntry{key, value});
  }
  return ZeroconfService{service.name, service.fullName, service.host, static_cast<double>(service.port), service.addresses, txt};
}

ZeroconfError ToJs(const rnzeroconf::Error &error) {
  return ZeroconfError{error.message, error.code, error.domain, error.serviceName};
}

} // namespace

ZeroconfModule::ZeroconfModule(std::shared_ptr<CallInvoker> jsInvoker, ZeroconfPlatform platform)
    : NativeZeroconfCxxSpec(std::move(jsInvoker)), platform_(std::move(platform)), emitter_(std::make_shared<Emitter>()) {
  emitter_->module = this;
}

ZeroconfModule::~ZeroconfModule() {
  {
    std::lock_guard<std::mutex> lock(emitter_->mutex);
    emitter_->module = nullptr;
  }
  // Stops the scans and unpublishes the services, on reload for example
  std::lock_guard<std::mutex> lock(backendsMutex_);
  for (auto &entry : backends_) {
    entry.second->Shutdown();
  }
}

std::shared_ptr<rnzeroconf::Backend> ZeroconfModule::Backend(const std::string &implType) {
  std::string key = platform_.backendKey ? platform_.backendKey(implType) : implType;
  std::lock_guard<std::mutex> lock(backendsMutex_);
  auto found = backends_.find(key);
  if (found != backends_.end()) {
    return found->second;
  }
  auto backend = platform_.createBackend(key, MakeEvents());
  backends_[key] = backend;
  return backend;
}

rnzeroconf::Events ZeroconfModule::MakeEvents() {
  std::weak_ptr<Emitter> weak = emitter_;
  // Runs an emit while the module exists
  auto with = [weak](const std::function<void(ZeroconfModule &)> &emit) {
    auto emitter = weak.lock();
    if (!emitter) {
      return;
    }
    std::lock_guard<std::mutex> lock(emitter->mutex);
    if (emitter->module) {
      emit(*emitter->module);
    }
  };

  rnzeroconf::Events events;
  events.start = [with](const std::string &scanId) {
    with([&](ZeroconfModule &module) { module.emitOnStart(ZeroconfScanEvent{scanId}); });
  };
  events.stop = [with](const std::string &scanId) {
    with([&](ZeroconfModule &module) { module.emitOnStop(ZeroconfScanEvent{scanId}); });
  };
  events.found = [with](const std::string &scanId, const std::string &name) {
    with([&](ZeroconfModule &module) { module.emitOnFound(ZeroconfNameEvent{scanId, name}); });
  };
  events.remove = [with](const std::string &scanId, const std::string &name) {
    with([&](ZeroconfModule &module) { module.emitOnRemove(ZeroconfNameEvent{scanId, name}); });
  };
  events.resolved = [with](const std::string &scanId, const rnzeroconf::Service &service) {
    with([&](ZeroconfModule &module) { module.emitOnResolved(ZeroconfResolvedEvent{scanId, ToJs(service)}); });
  };
  events.published = [with](const rnzeroconf::Service &service) {
    with([&](ZeroconfModule &module) { module.emitOnPublished(ToJs(service)); });
  };
  events.unpublished = [with](const rnzeroconf::Service &service) {
    with([&](ZeroconfModule &module) { module.emitOnUnpublished(ToJs(service)); });
  };
  events.error = [with](const std::string &scanId, const rnzeroconf::Error &error) {
    with([&](ZeroconfModule &module) { module.emitOnError(ZeroconfErrorEvent{scanId, ToJs(error)}); });
  };
  return events;
}

AsyncPromise<ZeroconfResult> ZeroconfModule::Settle(
    jsi::Runtime &rt,
    const std::function<void(rnzeroconf::ServiceCallback, rnzeroconf::ErrorCallback)> &call) {
  AsyncPromise<ZeroconfResult> promise(rt, jsInvoker_);
  call(
      [promise](const rnzeroconf::Service &service) mutable { promise.resolve(ZeroconfResult{ToJs(service), std::nullopt}); },
      [promise](const rnzeroconf::Error &error) mutable { promise.resolve(ZeroconfResult{std::nullopt, ToJs(error)}); });
  return promise;
}

void ZeroconfModule::scan(
    jsi::Runtime &,
    std::string scanId,
    std::string type,
    std::string protocol,
    std::string domain,
    std::string implType,
    ZeroconfScanOptions options) {
  rnzeroconf::ScanOptions scanOptions;
  scanOptions.resolveTimeoutSeconds = options.resolveTimeout.value_or(5);
  scanOptions.subtype = options.subtype.value_or("");
  scanOptions.networkInterface = options.networkInterface.value_or("");
  Backend(implType)->Scan(scanId, type, protocol, domain, scanOptions);
}

void ZeroconfModule::stop(jsi::Runtime &, std::string scanId, std::string implType) {
  Backend(implType)->Stop(scanId);
}

AsyncPromise<ZeroconfResult> ZeroconfModule::registerService(
    jsi::Runtime &rt,
    std::string type,
    std::string protocol,
    std::string domain,
    std::string name,
    double port,
    std::vector<ZeroconfTxtEntry> txt,
    std::string implType,
    ZeroconfPublishOptions options) {
  rnzeroconf::PublishOptions publishOptions;
  publishOptions.subtypes = options.subtypes.value_or(std::vector<std::string>{});
  publishOptions.networkInterface = options.networkInterface.value_or("");
  auto backend = Backend(implType);
  return Settle(rt, [&](rnzeroconf::ServiceCallback resolve, rnzeroconf::ErrorCallback reject) {
    backend->Publish(type, protocol, domain, name, static_cast<uint16_t>(port), ToTxt(txt), publishOptions, std::move(resolve), std::move(reject));
  });
}

AsyncPromise<ZeroconfResult> ZeroconfModule::updateService(jsi::Runtime &rt, std::string name, std::vector<ZeroconfTxtEntry> txt, std::string implType) {
  auto backend = Backend(implType);
  return Settle(rt, [&](rnzeroconf::ServiceCallback resolve, rnzeroconf::ErrorCallback reject) {
    backend->Update(name, ToTxt(txt), std::move(resolve), std::move(reject));
  });
}

AsyncPromise<ZeroconfResult> ZeroconfModule::unregisterService(jsi::Runtime &rt, std::string name, std::string implType) {
  auto backend = Backend(implType);
  return Settle(rt, [&](rnzeroconf::ServiceCallback resolve, rnzeroconf::ErrorCallback reject) {
    backend->Unpublish(name, std::move(resolve), std::move(reject));
  });
}

AsyncPromise<ZeroconfResult> ZeroconfModule::resolveService(
    jsi::Runtime &rt,
    std::string name,
    std::string type,
    std::string protocol,
    std::string domain,
    std::string implType,
    ZeroconfResolveOptions options) {
  rnzeroconf::ResolveOptions resolveOptions;
  resolveOptions.timeoutSeconds = options.timeout.value_or(5);
  resolveOptions.networkInterface = options.networkInterface.value_or("");
  auto backend = Backend(implType);
  return Settle(rt, [&](rnzeroconf::ServiceCallback resolve, rnzeroconf::ErrorCallback reject) {
    backend->ResolveService(name, type, protocol, domain, resolveOptions, std::move(resolve), std::move(reject));
  });
}

AsyncPromise<ZeroconfAccessResult> ZeroconfModule::checkLocalNetworkAccess(jsi::Runtime &rt, std::string type, double timeout) {
  AsyncPromise<ZeroconfAccessResult> promise(rt, jsInvoker_);
  if (!platform_.checkLocalNetworkAccess) {
    // Nothing to grant on this platform
    promise.resolve(ZeroconfAccessResult{std::string("granted"), std::nullopt});
    return promise;
  }
  platform_.checkLocalNetworkAccess(
      type,
      timeout,
      [promise](const std::string &status) mutable { promise.resolve(ZeroconfAccessResult{status, std::nullopt}); },
      [promise](const rnzeroconf::Error &error) mutable { promise.resolve(ZeroconfAccessResult{std::nullopt, ToJs(error)}); });
  return promise;
}

} // namespace facebook::react
