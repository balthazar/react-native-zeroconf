// The backend for the dns_sd API: Apple's system mDNSResponder (iOS, macOS, tvOS) and the mDNSResponder
// embedded in the library on Android (DNSSD).
#pragma once

#include "../Backend.h"
#include "Executor.h"

#include <map>
#include <memory>
#include <set>
#include <string>

namespace rnzeroconf {

struct DnssdScan;
struct DnssdResolve;
struct DnssdPublication;

class DnssdBackend : public Backend, public std::enable_shared_from_this<DnssdBackend> {
 public:
  // Callbacks of events run on the executor's thread
  static std::shared_ptr<DnssdBackend> Create(std::shared_ptr<Executor> executor, Events events);

  void Scan(
      const std::string &scanId,
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const ScanOptions &options) override;
  void Stop(const std::string &scanId) override;

  void Publish(
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const std::string &name,
      uint16_t port,
      const TxtPairs &txt,
      const PublishOptions &options,
      ServiceCallback resolve,
      ErrorCallback reject) override;
  void Unpublish(const std::string &name, ServiceCallback resolve, ErrorCallback reject) override;
  void Update(const std::string &name, const TxtPairs &txt, ServiceCallback resolve, ErrorCallback reject) override;

  void ResolveService(
      const std::string &name,
      const std::string &type,
      const std::string &protocol,
      const std::string &domain,
      const ResolveOptions &options,
      ServiceCallback resolve,
      ErrorCallback reject) override;

  void Shutdown() override;

  // dns_sd callbacks, on the executor's thread
  void OnBrowse(DnssdScan *scan, DNSServiceFlags flags, DNSServiceErrorType error, const char *name, const char *regtype, const char *domain);
  void OnResolve(DnssdResolve *resolve, DNSServiceErrorType error, const char *host, uint16_t port, uint16_t txtLength, const unsigned char *txt);
  void OnAddress(DnssdResolve *resolve, DNSServiceFlags flags, DNSServiceErrorType error, const struct sockaddr *address);
  void OnTxt(DnssdResolve *resolve, DNSServiceFlags flags, DNSServiceErrorType error, uint16_t length, const void *data);
  void OnRegister(DnssdPublication *publication, DNSServiceFlags flags, DNSServiceErrorType error, const char *name);

 private:
  DnssdBackend(std::shared_ptr<Executor> executor, Events events);

  // Runs on the executor's thread, while the backend exists
  void Run(std::function<void(DnssdBackend &)> task);

  void StopScan(const std::string &scanId);
  void StartResolve(const std::shared_ptr<DnssdScan> &scan, const std::string &name, const std::string &regtype, const std::string &domain);
  void BeginResolve(const std::shared_ptr<DnssdResolve> &resolve);
  void ResolveTimedOut(const std::shared_ptr<DnssdResolve> &resolve);
  void ScheduleEmit(const std::shared_ptr<DnssdResolve> &resolve);
  void EmitResolve(const std::shared_ptr<DnssdResolve> &resolve);
  void WatchTxt(const std::shared_ptr<DnssdResolve> &resolve);
  void FailResolve(const std::shared_ptr<DnssdResolve> &resolve, const Error &error);
  void CancelResolve(const std::shared_ptr<DnssdResolve> &resolve);
  void EndSingleResolve(const std::shared_ptr<DnssdResolve> &resolve);
  bool IsActive(const std::shared_ptr<DnssdResolve> &resolve) const;
  std::shared_ptr<DnssdResolve> Shared(DnssdResolve *resolve) const;
  // Deallocates a ref on the next turn, its callback may be running. keepAlive outlives the ref
  void DeallocateLater(DNSServiceRef &ref, std::shared_ptr<void> keepAlive);
  bool IsNameInUse(const std::string &name) const;
  Service PublishedService(const DnssdPublication &publication) const;
  void SendError(const std::string &scanId, const Error &error);

  std::shared_ptr<Executor> executor_;
  Events events_;
  std::map<std::string, std::shared_ptr<DnssdScan>> scans_;
  // Registered, keyed by the name actually used, which can differ from the requested one
  std::map<std::string, std::shared_ptr<DnssdPublication>> published_;
  // Waiting for the register callback
  std::set<std::shared_ptr<DnssdPublication>> pending_;
  // resolveService() calls in flight
  std::set<std::shared_ptr<DnssdResolve>> singleResolves_;
};

// Shared with the harness and the module

// Readable description of a DNSServiceErrorType
std::string DescribeDnssdError(DNSServiceErrorType code);
// { domain: 'DNSSD', code, message: "<action> <serviceName> failed: <description>" }
Error DnssdError(DNSServiceErrorType code, const std::string &action, const std::string &serviceName = "");
// TXT record bytes <-> ordered pairs. Entries longer than 255 bytes are left out and their keys listed in tooLong
std::string EncodeTxt(const TxtPairs &txt, std::vector<std::string> *tooLong);
TxtPairs DecodeTxt(const void *record, uint16_t length);
// The index of a network interface by name ("en0"), kDNSServiceInterfaceIndexAny for an empty name.
// False when the interface doesn't exist
bool InterfaceIndex(const std::string &name, uint32_t &index);

} // namespace rnzeroconf
