// Zeroconf on Windows with the DNS-SD functions of windns.h (Windows 10 1809 and later).
// No React Native dependency: RNZeroconf.cpp bridges it to JavaScript, test/windows/harness.cpp tests it.
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windns.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace rnzeroconf {

using TxtPairs = std::vector<std::pair<std::wstring, std::wstring>>;

struct Service {
  std::wstring name;
  std::wstring fullName;
  std::wstring host;
  uint16_t port = 0;
  std::vector<std::wstring> addresses;
  TxtPairs txt;
};

// { message, code, domain, serviceName } as on the other platforms.
// domain "Windows": code is a Win32 / DNS_STATUS number. domain "RNZeroconf": stringCode is set.
struct Error {
  std::wstring domain;
  int64_t code = 0;
  std::wstring stringCode;
  std::wstring message;
  std::wstring serviceName;
};

using ServiceCallback = std::function<void(const Service &)>;
using ErrorCallback = std::function<void(const Error &)>;

// Called from DNS-SD threads. scanId is empty for errors that are not about a scan.
struct Events {
  std::function<void(const std::string &scanId)> start;
  std::function<void(const std::string &scanId)> stop;
  std::function<void(const std::string &scanId, const std::wstring &name)> found;
  std::function<void(const std::string &scanId, const std::wstring &name)> remove;
  std::function<void(const std::string &scanId, const Service &)> resolved;
  std::function<void(const Service &)> published;
  std::function<void(const Service &)> unpublished;
  std::function<void(const std::string &scanId, const Error &)> error;
};

struct Operation;
struct Browse;
struct Resolve;
struct Publication;

class Zeroconf {
 public:
  explicit Zeroconf(Events events);
  ~Zeroconf();

  Zeroconf(const Zeroconf &) = delete;
  Zeroconf &operator=(const Zeroconf &) = delete;

  // type and protocol without underscore ("http", "tcp"). "services._dns-sd" / "udp" lists service types
  void Scan(
      const std::string &scanId,
      const std::wstring &type,
      const std::wstring &protocol,
      const std::wstring &domain,
      const std::wstring &subtype,
      const std::wstring &networkInterface);
  void Stop(const std::string &scanId);
  void StopAll();

  void Publish(
      const std::wstring &type,
      const std::wstring &protocol,
      const std::wstring &domain,
      const std::wstring &name,
      uint16_t port,
      const TxtPairs &txt,
      const std::wstring &networkInterface,
      ServiceCallback resolve,
      ErrorCallback reject);
  void Unpublish(const std::wstring &name, ServiceCallback resolve, ErrorCallback reject);
  // windns.h has no update: unpublishes and publishes again under the same name
  void Update(const std::wstring &name, const TxtPairs &txt, ServiceCallback resolve, ErrorCallback reject);
  void UnpublishAll();

  void ResolveService(
      const std::wstring &name,
      const std::wstring &type,
      const std::wstring &protocol,
      const std::wstring &domain,
      const std::wstring &networkInterface,
      double timeoutSeconds,
      ServiceCallback resolve,
      ErrorCallback reject);

  // Callbacks of windns.h, routed through a registry of live operations
  void OnBrowse(const std::shared_ptr<Browse> &browse, DWORD status, PDNS_RECORD records);
  void OnResolve(const std::shared_ptr<Resolve> &resolve, DWORD status, PDNS_SERVICE_INSTANCE instance);
  void OnRegister(const std::shared_ptr<Publication> &publication, DWORD status, PDNS_SERVICE_INSTANCE instance);

 private:
  using After = std::vector<std::function<void()>>;

  void StopLocked(const std::string &scanId, After &after);
  void StartResolve(const std::shared_ptr<Resolve> &resolve, After &after);
  void CancelResolve(const std::shared_ptr<Resolve> &resolve, After &after);
  void PublishAs(
      const std::wstring &regType,
      const std::wstring &domain,
      const std::wstring &requestedName,
      uint16_t port,
      const TxtPairs &txt,
      ULONG interfaceIndex,
      bool announce,
      ServiceCallback resolve,
      ErrorCallback reject);
  void UnpublishWith(const std::wstring &name, bool announce, ServiceCallback resolve, ErrorCallback reject);
  Service PublishedService(const Publication &publication) const;
  bool InterfaceIndex(const std::wstring &networkInterface, ULONG &index, Error &error) const;

  Events events_;
  // False once destroyed, for the resolveService() timeout threads
  std::shared_ptr<std::atomic<bool>> alive_;
  std::recursive_mutex mutex_;
  std::map<std::string, std::shared_ptr<Browse>> browses_;
  // Keyed by the published name, which can differ from the requested one
  std::map<std::wstring, std::shared_ptr<Publication>> publications_;
  std::vector<std::shared_ptr<Resolve>> singleResolves_;
  std::wstring hostName_;
};

// Helpers shared with the bridge and the harness
std::wstring Widen(const std::string &value);
std::string Narrow(const std::wstring &value);
std::wstring DescribeStatus(DWORD status);

} // namespace rnzeroconf
