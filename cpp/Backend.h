// What a platform implements: scanning, resolving and publishing on one mDNS stack.
// No React Native dependency: ZeroconfModule bridges a backend to JavaScript, the native harnesses test it directly.
// Strings are UTF-8. Callbacks run on the backend's own thread.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rnzeroconf {

// TXT records as ordered [key, value] pairs
using TxtPairs = std::vector<std::pair<std::string, std::string>>;

struct Service {
  std::string name;
  std::string fullName;
  std::string host;
  uint16_t port = 0;
  std::vector<std::string> addresses;
  TxtPairs txt;
};

// { message, code, domain, serviceName } as JavaScript sees it.
// The code is a string for domain "RNZeroconf" ('TIMEOUT'), the platform's number otherwise (-65555)
struct Error {
  std::string domain;
  std::string code;
  std::string message;
  std::string serviceName;
};

Error LibraryError(const std::string &code, const std::string &message, const std::string &serviceName = "");

using ServiceCallback = std::function<void(const Service &)>;
using ErrorCallback = std::function<void(const Error &)>;

// Scan events carry the id of the scan they belong to. An error with an empty scanId is not about a scan
struct Events {
  std::function<void(const std::string &scanId)> start;
  std::function<void(const std::string &scanId)> stop;
  std::function<void(const std::string &scanId, const std::string &name)> found;
  std::function<void(const std::string &scanId, const std::string &name)> remove;
  std::function<void(const std::string &scanId, const Service &)> resolved;
  std::function<void(const Service &)> published;
  std::function<void(const Service &)> unpublished;
  std::function<void(const std::string &scanId, const Error &)> error;
};

struct ScanOptions {
  // "printer" or "_printer", empty for none
  std::string subtype;
  // "en0", empty for every interface
  std::string networkInterface;
  // Each found service is resolved for this long, retried once, then reported with a TIMEOUT error
  double resolveTimeoutSeconds = 5;
};

struct PublishOptions {
  std::vector<std::string> subtypes;
  std::string networkInterface;
};

struct ResolveOptions {
  std::string networkInterface;
  double timeoutSeconds = 5;
};

class Backend {
 public:
  virtual ~Backend() = default;

  // type and protocol without underscore ("http", "tcp"); "services._dns-sd" / "udp" lists the service types.
  // Starting a scan with the id of a running one replaces it
  virtual void Scan(
      const std::string &scanId,
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const ScanOptions &options) = 0;
  // Stops the scan with this id, or every scan with an empty one
  virtual void Stop(const std::string &scanId) = 0;

  virtual void Publish(
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const std::string &name,
      uint16_t port,
      const TxtPairs &txt,
      const PublishOptions &options,
      ServiceCallback resolve,
      ErrorCallback reject) = 0;
  virtual void Unpublish(const std::string &name, ServiceCallback resolve, ErrorCallback reject) = 0;
  virtual void Update(const std::string &name, const TxtPairs &txt, ServiceCallback resolve, ErrorCallback reject) = 0;

  virtual void ResolveService(
      const std::string &name,
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const ResolveOptions &options,
      ServiceCallback resolve,
      ErrorCallback reject) = 0;

  // Stops the scans and unpublishes the services, when the module goes away
  virtual void Shutdown() = 0;
};

// "printer" or "_printer" -> "_printer", empty stays empty
std::string SubtypeLabel(const std::string &subtype);

} // namespace rnzeroconf
