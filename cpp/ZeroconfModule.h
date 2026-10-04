// The native module, one C++ TurboModule for every platform. It bridges a platform's backends
// (cpp/Backend.h) to JavaScript: calls go to the backend for the call's implType, backend callbacks
// become events and settled promises.
#pragma once

#include <RNZeroconfSpecJSI.h>

#include "Backend.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace facebook::react {

using ZeroconfTxtEntry = NativeZeroconfTxtEntry<std::string, std::string>;
template <>
struct Bridging<ZeroconfTxtEntry> : NativeZeroconfTxtEntryBridging<ZeroconfTxtEntry> {};

using ZeroconfService = NativeZeroconfNativeService<std::string, std::string, std::string, double, std::vector<std::string>, std::vector<ZeroconfTxtEntry>>;
template <>
struct Bridging<ZeroconfService> : NativeZeroconfNativeServiceBridging<ZeroconfService> {};

using ZeroconfError = NativeZeroconfNativeError<std::string, std::string, std::string, std::string>;
template <>
struct Bridging<ZeroconfError> : NativeZeroconfNativeErrorBridging<ZeroconfError> {};

using ZeroconfResult = NativeZeroconfNativeResult<std::optional<ZeroconfService>, std::optional<ZeroconfError>>;
template <>
struct Bridging<ZeroconfResult> : NativeZeroconfNativeResultBridging<ZeroconfResult> {};

using ZeroconfAccessResult = NativeZeroconfNativeAccessResult<std::optional<std::string>, std::optional<ZeroconfError>>;
template <>
struct Bridging<ZeroconfAccessResult> : NativeZeroconfNativeAccessResultBridging<ZeroconfAccessResult> {};

using ZeroconfScanEvent = NativeZeroconfScanEvent<std::string>;
template <>
struct Bridging<ZeroconfScanEvent> : NativeZeroconfScanEventBridging<ZeroconfScanEvent> {};

using ZeroconfNameEvent = NativeZeroconfNameEvent<std::string, std::string>;
template <>
struct Bridging<ZeroconfNameEvent> : NativeZeroconfNameEventBridging<ZeroconfNameEvent> {};

using ZeroconfResolvedEvent = NativeZeroconfResolvedEvent<std::string, ZeroconfService>;
template <>
struct Bridging<ZeroconfResolvedEvent> : NativeZeroconfResolvedEventBridging<ZeroconfResolvedEvent> {};

using ZeroconfErrorEvent = NativeZeroconfErrorEvent<std::string, ZeroconfError>;
template <>
struct Bridging<ZeroconfErrorEvent> : NativeZeroconfErrorEventBridging<ZeroconfErrorEvent> {};

using ZeroconfScanOptions = NativeZeroconfNativeScanOptions<std::optional<double>, std::optional<std::string>, std::optional<std::string>>;
template <>
struct Bridging<ZeroconfScanOptions> : NativeZeroconfNativeScanOptionsBridging<ZeroconfScanOptions> {};

using ZeroconfPublishOptions = NativeZeroconfNativePublishOptions<std::optional<std::vector<std::string>>, std::optional<std::string>>;
template <>
struct Bridging<ZeroconfPublishOptions> : NativeZeroconfNativePublishOptionsBridging<ZeroconfPublishOptions> {};

using ZeroconfResolveOptions = NativeZeroconfNativeResolveOptions<std::optional<double>, std::optional<std::string>>;
template <>
struct Bridging<ZeroconfResolveOptions> : NativeZeroconfNativeResolveOptionsBridging<ZeroconfResolveOptions> {};

// What a platform provides to the module, set before the module is created
struct ZeroconfPlatform {
  // The backend for a key; the platform maps implTypes to keys, so platforms with one backend ignore implType
  std::function<std::string(const std::string &implType)> backendKey;
  std::function<std::shared_ptr<rnzeroconf::Backend>(const std::string &key, rnzeroconf::Events events)> createBackend;
  // Settles with 'granted', 'denied' or 'unknown', or an error
  std::function<void(
      const std::string &type,
      double timeoutSeconds,
      std::function<void(const std::string &status)> resolve,
      std::function<void(const rnzeroconf::Error &error)> reject)>
      checkLocalNetworkAccess;
  // Called when the module starts or stops having anything running: scans, published services,
  // publishes and resolves in flight. Android holds its multicast lock in between
  std::function<void(bool active)> setActive;
};

class ZeroconfModule : public NativeZeroconfCxxSpec<ZeroconfModule> {
 public:
  ZeroconfModule(std::shared_ptr<CallInvoker> jsInvoker, ZeroconfPlatform platform);
  ~ZeroconfModule() override;

  void scan(jsi::Runtime &rt, std::string scanId, std::string type, std::string protocol, std::string domain, std::string implType, ZeroconfScanOptions options);
  void stop(jsi::Runtime &rt, std::string scanId, std::string implType);
  AsyncPromise<ZeroconfResult> registerService(
      jsi::Runtime &rt,
      std::string type,
      std::string protocol,
      std::string domain,
      std::string name,
      double port,
      std::vector<ZeroconfTxtEntry> txt,
      std::string implType,
      ZeroconfPublishOptions options);
  AsyncPromise<ZeroconfResult> updateService(jsi::Runtime &rt, std::string name, std::vector<ZeroconfTxtEntry> txt, std::string implType);
  AsyncPromise<ZeroconfResult> unregisterService(jsi::Runtime &rt, std::string name, std::string implType);
  AsyncPromise<ZeroconfResult> resolveService(
      jsi::Runtime &rt,
      std::string name,
      std::string type,
      std::string protocol,
      std::string domain,
      std::string implType,
      ZeroconfResolveOptions options);
  AsyncPromise<ZeroconfAccessResult> checkLocalNetworkAccess(jsi::Runtime &rt, std::string type, double timeout);

 private:
  // Backend callbacks can outlive the module, they go through this
  struct Emitter {
    std::mutex mutex;
    ZeroconfModule *module = nullptr;
  };

  std::shared_ptr<rnzeroconf::Backend> Backend(const std::string &implType);
  rnzeroconf::Events MakeEvents();
  AsyncPromise<ZeroconfResult> Settle(
      jsi::Runtime &rt,
      const std::function<void(rnzeroconf::ServiceCallback, rnzeroconf::ErrorCallback)> &call,
      bool active = false);

  // What is running, for ZeroconfPlatform::setActive
  enum class Activity { ScanStarted, ScanStopped, AllScansStopped, Published, Unpublished, CallStarted, CallSettled };
  void Track(Activity activity, const std::string &key = "");

  ZeroconfPlatform platform_;
  std::shared_ptr<Emitter> emitter_;
  std::mutex backendsMutex_;
  std::map<std::string, std::shared_ptr<rnzeroconf::Backend>> backends_;
  std::mutex activityMutex_;
  std::set<std::string> runningScans_;
  std::set<std::string> publishedServices_;
  int callsInFlight_ = 0;
  bool active_ = false;
};

} // namespace facebook::react
