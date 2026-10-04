#include "WindowsBackend.h"

#include "ZeroconfCore.h"

#include <utility>

namespace rnzeroconf {

namespace {

std::wstring Wide(const std::string &value) {
  return win::Widen(value);
}

Service FromCore(const win::Service &service) {
  Service converted;
  converted.name = win::Narrow(service.name);
  converted.fullName = win::Narrow(service.fullName);
  converted.host = win::Narrow(service.host);
  converted.port = service.port;
  for (const auto &address : service.addresses) {
    converted.addresses.push_back(win::Narrow(address));
  }
  for (const auto &[key, value] : service.txt) {
    converted.txt.emplace_back(win::Narrow(key), win::Narrow(value));
  }
  return converted;
}

// The core's codes are numbers for domain "Windows" (Win32 / DNS_STATUS), strings for "RNZeroconf"
Error FromCore(const win::Error &error) {
  return Error{
      win::Narrow(error.domain),
      error.stringCode.empty() ? std::to_string(error.code) : win::Narrow(error.stringCode),
      win::Narrow(error.message),
      win::Narrow(error.serviceName)};
}

win::TxtPairs ToCore(const TxtPairs &txt) {
  win::TxtPairs converted;
  for (const auto &[key, value] : txt) {
    converted.emplace_back(Wide(key), Wide(value));
  }
  return converted;
}

win::ServiceCallback ToCore(ServiceCallback resolve) {
  return [resolve = std::move(resolve)](const win::Service &service) { resolve(FromCore(service)); };
}

win::ErrorCallback ToCore(ErrorCallback reject) {
  return [reject = std::move(reject)](const win::Error &error) { reject(FromCore(error)); };
}

class WindowsBackend : public Backend {
 public:
  explicit WindowsBackend(Events events) : core_(std::make_unique<win::Zeroconf>(CoreEvents(std::move(events)))) {}

  void Scan(
      const std::string &scanId,
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const ScanOptions &options) override {
    core_->Scan(scanId, Wide(type), Wide(protocol), Wide(domain), Wide(options.subtype), Wide(options.networkInterface),
                options.resolveTimeoutSeconds);
  }

  void Stop(const std::string &scanId) override {
    if (scanId.empty()) {
      core_->StopAll();
    } else {
      core_->Stop(scanId);
    }
  }

  void Publish(
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const std::string &name,
      uint16_t port,
      const TxtPairs &txt,
      const PublishOptions &options,
      ServiceCallback resolve,
      ErrorCallback reject) override {
    // windns.h can't register subtypes
    if (!options.subtypes.empty()) {
      reject(LibraryError("UNSUPPORTED", "Publishing subtypes is not supported on Windows", name));
      return;
    }
    core_->Publish(Wide(type), Wide(protocol), Wide(domain), Wide(name), port, ToCore(txt), Wide(options.networkInterface),
                   ToCore(std::move(resolve)), ToCore(std::move(reject)));
  }

  void Unpublish(const std::string &name, ServiceCallback resolve, ErrorCallback reject) override {
    core_->Unpublish(Wide(name), ToCore(std::move(resolve)), ToCore(std::move(reject)));
  }

  void Update(const std::string &name, const TxtPairs &txt, ServiceCallback resolve, ErrorCallback reject) override {
    core_->Update(Wide(name), ToCore(txt), ToCore(std::move(resolve)), ToCore(std::move(reject)));
  }

  void ResolveService(
      const std::string &name,
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const ResolveOptions &options,
      ServiceCallback resolve,
      ErrorCallback reject) override {
    core_->ResolveService(Wide(name), Wide(type), Wide(protocol), Wide(domain), Wide(options.networkInterface), options.timeoutSeconds,
                          ToCore(std::move(resolve)), ToCore(std::move(reject)));
  }

  void Shutdown() override {
    core_->StopAll();
    core_->UnpublishAll();
  }

 private:
  static win::Events CoreEvents(Events events) {
    win::Events core;
    core.start = events.start;
    core.stop = events.stop;
    core.found = [found = events.found](const std::string &scanId, const std::wstring &name) {
      if (found) found(scanId, win::Narrow(name));
    };
    core.remove = [remove = events.remove](const std::string &scanId, const std::wstring &name) {
      if (remove) remove(scanId, win::Narrow(name));
    };
    core.resolved = [resolved = events.resolved](const std::string &scanId, const win::Service &service) {
      if (resolved) resolved(scanId, FromCore(service));
    };
    core.published = [published = events.published](const win::Service &service) {
      if (published) published(FromCore(service));
    };
    core.unpublished = [unpublished = events.unpublished](const win::Service &service) {
      if (unpublished) unpublished(FromCore(service));
    };
    core.error = [error = events.error](const std::string &scanId, const win::Error &value) {
      if (error) error(scanId, FromCore(value));
    };
    return core;
  }

  std::unique_ptr<win::Zeroconf> core_;
};

} // namespace

std::shared_ptr<Backend> CreateWindowsBackend(Events events) {
  return std::make_shared<WindowsBackend>(std::move(events));
}

} // namespace rnzeroconf
