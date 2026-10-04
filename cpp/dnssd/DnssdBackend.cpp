#include "DnssdBackend.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <vector>

namespace rnzeroconf {

// A browse, keyed by the id of the JS instance that started it, several can run at once
struct DnssdScan {
  std::weak_ptr<DnssdBackend> owner;
  std::weak_ptr<DnssdScan> self;
  std::string scanId;
  DNSServiceRef browseRef = nullptr;
  double resolveTimeoutSeconds = 5;
  uint32_t interfaceIndex = kDNSServiceInterfaceIndexAny;
  // Services are reported once per network interface, count them so found/remove are emitted once
  std::map<std::string, int> foundInterfaces;
  std::map<std::string, std::shared_ptr<DnssdResolve>> resolving;
  // Browsing _services._dns-sd._udp: results are service types, reported as "_http._tcp" and not resolved
  bool typesOnly = false;
};

// A found service being resolved: DNSServiceResolve for host, port and TXT, then DNSServiceGetAddrInfo for
// addresses. Once resolved, the address query and a TXT record query stay open and changes are emitted again.
// Without a scan, it is a single resolveService() call settling its callbacks.
struct DnssdResolve {
  std::weak_ptr<DnssdBackend> owner;
  std::weak_ptr<DnssdResolve> self;
  std::weak_ptr<DnssdScan> scan;
  ServiceCallback promiseResolve;
  ErrorCallback promiseReject;
  bool single = false;
  uint32_t interfaceIndex = kDNSServiceInterfaceIndexAny;
  double timeoutSeconds = 5;
  DNSServiceRef resolveRef = nullptr;
  DNSServiceRef addressRef = nullptr;
  DNSServiceRef txtRef = nullptr;
  bool emitted = false;
  std::optional<Service> lastEmitted;
  std::string name;
  std::string regtype;
  std::string domain;
  std::string host;
  uint16_t port = 0;
  TxtPairs txt;
  std::vector<std::string> addresses;
  bool retried = false;
  bool finished = false;
  Executor::TimerId timeoutTimer = 0;
  Executor::TimerId emitTimer = 0;
};

// A service registered with DNSServiceRegister
struct DnssdPublication {
  std::weak_ptr<DnssdBackend> owner;
  std::weak_ptr<DnssdPublication> self;
  std::string name;
  std::string regtype;
  std::string domain;
  uint16_t port = 0;
  TxtPairs txt;
  DNSServiceRef ref = nullptr;
  ServiceCallback resolve;
  ErrorCallback reject;
};

namespace {

bool operator==(const Service &a, const Service &b) {
  return a.name == b.name && a.fullName == b.fullName && a.host == b.host && a.port == b.port &&
      a.addresses == b.addresses && a.txt == b.txt;
}

// dns_sd callbacks copy what they receive and hand it to the executor: the embedded mDNSResponder of Android
// calls back on its own thread (and sometimes from inside the call that starts the operation), Apple's on
// the executor's queue. Either way the operation is looked up again before it is used.
template <typename Operation, typename Handler>
void Deliver(Operation *operation, Handler handler) {
  auto owner = operation->owner.lock();
  if (!owner) {
    return;
  }
  owner->Post([weakOwner = operation->owner, weakOperation = operation->self, handler = std::move(handler)] {
    auto backend = weakOwner.lock();
    auto current = weakOperation.lock();
    if (backend && current) {
      handler(*backend, current);
    }
  });
}

std::string AddressString(const struct sockaddr *address);

void DNSSD_API BrowseReply(DNSServiceRef, DNSServiceFlags flags, uint32_t, DNSServiceErrorType error, const char *name,
                           const char *regtype, const char *domain, void *context) {
  std::optional<std::string> nameCopy = name ? std::optional<std::string>(name) : std::nullopt;
  std::string regtypeCopy = regtype ? regtype : "";
  std::string domainCopy = domain ? domain : "";
  Deliver(static_cast<DnssdScan *>(context), [=](DnssdBackend &backend, const std::shared_ptr<DnssdScan> &scan) {
    backend.OnBrowse(scan, flags, error, nameCopy, regtypeCopy, domainCopy);
  });
}

void DNSSD_API ResolveReply(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType error, const char *,
                            const char *host, uint16_t port, uint16_t txtLength, const unsigned char *txt, void *context) {
  std::string hostCopy = host ? host : "";
  std::string txtCopy = txt ? std::string(reinterpret_cast<const char *>(txt), txtLength) : "";
  uint16_t hostPort = ntohs(port);
  Deliver(static_cast<DnssdResolve *>(context), [=](DnssdBackend &backend, const std::shared_ptr<DnssdResolve> &resolve) {
    backend.OnResolve(resolve, error, hostCopy, hostPort, txtCopy);
  });
}

void DNSSD_API AddressReply(DNSServiceRef, DNSServiceFlags flags, uint32_t, DNSServiceErrorType error, const char *,
                            const struct sockaddr *address, uint32_t, void *context) {
  std::string addressCopy = AddressString(address);
  Deliver(static_cast<DnssdResolve *>(context), [=](DnssdBackend &backend, const std::shared_ptr<DnssdResolve> &resolve) {
    backend.OnAddress(resolve, flags, error, addressCopy);
  });
}

void DNSSD_API TxtReply(DNSServiceRef, DNSServiceFlags flags, uint32_t, DNSServiceErrorType error, const char *,
                        uint16_t, uint16_t, uint16_t length, const void *data, uint32_t, void *context) {
  std::string dataCopy = data ? std::string(static_cast<const char *>(data), length) : "";
  Deliver(static_cast<DnssdResolve *>(context), [=](DnssdBackend &backend, const std::shared_ptr<DnssdResolve> &resolve) {
    backend.OnTxt(resolve, flags, error, dataCopy);
  });
}

void DNSSD_API RegisterReply(DNSServiceRef, DNSServiceFlags flags, DNSServiceErrorType error, const char *name,
                             const char *, const char *, void *context) {
  std::optional<std::string> nameCopy = name ? std::optional<std::string>(name) : std::nullopt;
  Deliver(static_cast<DnssdPublication *>(context), [=](DnssdBackend &backend, const std::shared_ptr<DnssdPublication> &publication) {
    backend.OnRegister(publication, flags, error, nameCopy);
  });
}

std::string AddressString(const struct sockaddr *address) {
  if (address == nullptr) {
    return "";
  }
  char buffer[INET6_ADDRSTRLEN] = {};
  const char *result = nullptr;
  if (address->sa_family == AF_INET) {
    result = inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in *>(address)->sin_addr, buffer, sizeof(buffer));
  } else if (address->sa_family == AF_INET6) {
    result = inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6 *>(address)->sin6_addr, buffer, sizeof(buffer));
  }
  return result ? std::string(result) : "";
}

// TXT values are UTF-8 by convention, older devices send Latin-1
bool IsUtf8(const std::string &value) {
  size_t i = 0;
  while (i < value.size()) {
    auto c = static_cast<unsigned char>(value[i]);
    size_t extra;
    if (c < 0x80) {
      extra = 0;
    } else if ((c >> 5) == 0x6) {
      extra = 1;
    } else if ((c >> 4) == 0xE) {
      extra = 2;
    } else if ((c >> 3) == 0x1E) {
      extra = 3;
    } else {
      return false;
    }
    if (i + extra >= value.size() && extra > 0) {
      return false;
    }
    for (size_t k = 1; k <= extra; k++) {
      if ((static_cast<unsigned char>(value[i + k]) >> 6) != 0x2) {
        return false;
      }
    }
    i += extra + 1;
  }
  return true;
}

std::string Latin1ToUtf8(const std::string &value) {
  std::string out;
  for (unsigned char c : value) {
    if (c < 0x80) {
      out += static_cast<char>(c);
    } else {
      out += static_cast<char>(0xC0 | (c >> 6));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
  }
  return out;
}

Error BrowseError(DNSServiceErrorType code, bool typesOnly) {
  if (!typesOnly) {
    return DnssdError(code, "Browsing services");
  }
  if (code == kDNSServiceErr_NoAuth) {
    // Browsing for service types requires the multicast entitlement (Apple's Local Network Privacy FAQ)
    return Error{"DNSSD", std::to_string(code),
                 "Listing service types failed: not authorized, it requires the com.apple.developer.networking.multicast entitlement", ""};
  }
  return DnssdError(code, "Listing service types");
}

Error NotPublished(const std::string &name) {
  return LibraryError("NOT_PUBLISHED", "Service " + name + " is not published", name);
}

Error UnknownInterface(const std::string &name) {
  return LibraryError("UNKNOWN_INTERFACE", "Unknown network interface " + name);
}

} // namespace

std::string DescribeDnssdError(DNSServiceErrorType code) {
  switch (code) {
    case kDNSServiceErr_NoAuth:
      return "not authorized, add the service type to NSBonjourServices in Info.plist";
    // kDNSServiceErr_PolicyDenied, missing from the embedded responder's older dns_sd.h
    case -65570:
      return "Local Network access denied";
    case kDNSServiceErr_NameConflict:
      return "name already in use";
    case kDNSServiceErr_BadParam:
      return "bad parameter";
    case kDNSServiceErr_NoSuchName:
      return "no such name";
    case kDNSServiceErr_NoSuchRecord:
      return "no such record";
    case kDNSServiceErr_ServiceNotRunning:
      return "mDNSResponder is not running";
    case kDNSServiceErr_Timeout:
      return "timed out";
    case kDNSServiceErr_NoMemory:
      return "out of memory";
    case kDNSServiceErr_Unsupported:
      return "unsupported";
    default:
      return "error " + std::to_string(code);
  }
}

Error DnssdError(DNSServiceErrorType code, const std::string &action, const std::string &serviceName) {
  std::string subject = serviceName.empty() ? action : action + " " + serviceName;
  return Error{"DNSSD", std::to_string(code), subject + " failed: " + DescribeDnssdError(code), serviceName};
}

std::string EncodeTxt(const TxtPairs &txt, std::vector<std::string> *tooLong) {
  std::string record;
  for (const auto &[key, value] : txt) {
    std::string entry = key + "=" + value;
    if (entry.size() > 255) {
      if (tooLong) {
        tooLong->push_back(key);
      }
      continue;
    }
    record += static_cast<char>(entry.size());
    record += entry;
  }
  return record;
}

TxtPairs DecodeTxt(const void *record, uint16_t length) {
  TxtPairs txt;
  if (record == nullptr || length == 0) {
    return txt;
  }
  uint16_t count = TXTRecordGetCount(length, record);
  for (uint16_t i = 0; i < count; i++) {
    char key[256] = {};
    uint8_t valueLength = 0;
    const void *value = nullptr;
    if (TXTRecordGetItemAtIndex(length, record, i, sizeof(key), key, &valueLength, &value) != kDNSServiceErr_NoError || key[0] == '\0') {
      continue;
    }
    std::string string = value != nullptr && valueLength > 0 ? std::string(static_cast<const char *>(value), valueLength) : "";
    txt.emplace_back(key, IsUtf8(string) ? string : Latin1ToUtf8(string));
  }
  return txt;
}

bool InterfaceIndex(const std::string &name, uint32_t &index) {
  index = kDNSServiceInterfaceIndexAny;
  if (name.empty()) {
    return true;
  }
  unsigned int found = if_nametoindex(name.c_str());
  if (found == 0) {
    return false;
  }
  index = found;
  return true;
}

std::shared_ptr<DnssdBackend> DnssdBackend::Create(std::shared_ptr<Executor> executor, Events events) {
  return std::shared_ptr<DnssdBackend>(new DnssdBackend(std::move(executor), std::move(events)));
}

DnssdBackend::DnssdBackend(std::shared_ptr<Executor> executor, Events events)
    : executor_(std::move(executor)), events_(std::move(events)) {}

void DnssdBackend::Post(Executor::Task task) {
  executor_->Post(std::move(task));
}

void DnssdBackend::Run(std::function<void(DnssdBackend &)> task) {
  std::weak_ptr<DnssdBackend> weak = weak_from_this();
  executor_->Post([weak, task = std::move(task)] {
    if (auto self = weak.lock()) {
      task(*self);
    }
  });
}

void DnssdBackend::SendError(const std::string &scanId, const Error &error) {
  if (events_.error) {
    events_.error(scanId, error);
  }
}

void DnssdBackend::DeallocateLater(DNSServiceRef &ref, std::shared_ptr<void> keepAlive) {
  if (ref == nullptr) {
    return;
  }
  DNSServiceRef deallocated = ref;
  ref = nullptr;
  executor_->Post([deallocated, keepAlive = std::move(keepAlive)] { DNSServiceRefDeallocate(deallocated); });
}

// Scan

void DnssdBackend::Scan(
    const std::string &scanId,
    const std::string &type,
    const std::string &protocol,
    const std::string &domain,
    const ScanOptions &options) {
  Run([=](DnssdBackend &self) {
    self.StopScan(scanId);

    auto scan = std::make_shared<DnssdScan>();
    scan->owner = self.weak_from_this();
    scan->self = scan;
    scan->scanId = scanId;
    scan->resolveTimeoutSeconds = options.resolveTimeoutSeconds > 0 ? options.resolveTimeoutSeconds : 5;
    if (!InterfaceIndex(options.networkInterface, scan->interfaceIndex)) {
      self.SendError(scanId, UnknownInterface(options.networkInterface));
      return;
    }

    std::string regtype = "_" + type + "._" + protocol;
    scan->typesOnly = regtype == "_services._dns-sd._udp";
    // "_ipp._tcp,_printer" browses the _printer subtype
    std::string browseType = regtype;
    std::string subtype = SubtypeLabel(options.subtype);
    if (!subtype.empty()) {
      browseType += "," + subtype;
    }
    DNSServiceErrorType error = DNSServiceBrowse(&scan->browseRef, 0, scan->interfaceIndex, browseType.c_str(),
                                                 domain.empty() ? nullptr : domain.c_str(), BrowseReply, scan.get());
    if (error != kDNSServiceErr_NoError) {
      scan->browseRef = nullptr;
      self.SendError(scanId, BrowseError(error, scan->typesOnly));
      return;
    }
    self.executor_->Attach(scan->browseRef);
    self.scans_[scanId] = scan;
    if (self.events_.start) {
      self.events_.start(scanId);
    }
  });
}

void DnssdBackend::Stop(const std::string &scanId) {
  Run([scanId](DnssdBackend &self) {
    if (!scanId.empty()) {
      self.StopScan(scanId);
      return;
    }
    std::vector<std::string> ids;
    for (const auto &entry : self.scans_) {
      ids.push_back(entry.first);
    }
    for (const auto &id : ids) {
      self.StopScan(id);
    }
  });
}

void DnssdBackend::StopScan(const std::string &scanId) {
  auto found = scans_.find(scanId);
  if (found == scans_.end()) {
    return;
  }
  auto scan = found->second;
  scans_.erase(found);

  for (auto &entry : scan->resolving) {
    CancelResolve(entry.second);
  }
  scan->resolving.clear();
  scan->foundInterfaces.clear();
  if (scan->browseRef) {
    DNSServiceRefDeallocate(scan->browseRef);
    scan->browseRef = nullptr;
  }
  if (events_.stop) {
    events_.stop(scanId);
  }
}

void DnssdBackend::OnBrowse(const std::shared_ptr<DnssdScan> &scan, DNSServiceFlags flags, DNSServiceErrorType error, const std::optional<std::string> &name, const std::string &regtype, const std::string &domain) {
  if (scan->browseRef == nullptr) {
    return;
  }
  auto found = scans_.find(scan->scanId);
  if (found == scans_.end() || found->second != scan) {
    return;
  }

  if (error != kDNSServiceErr_NoError) {
    SendError(scan->scanId, BrowseError(error, scan->typesOnly));
    // The browse can't continue after an error
    DeallocateLater(scan->browseRef, scan);
    StopScan(scan->scanId);
    return;
  }
  if (!name) {
    return;
  }
  std::string serviceName = *name;
  if (scan->typesOnly) {
    // name is "_http" and regtype "_tcp.local.", the service type is "_http._tcp"
    std::string protocol = regtype.substr(0, regtype.find('.'));
    // Subtypes are announced the same way ("_printer._sub"), only types are listed
    if (protocol != "_tcp" && protocol != "_udp") {
      return;
    }
    serviceName += "." + protocol;
  }
  int interfaces = scan->foundInterfaces.count(serviceName) ? scan->foundInterfaces[serviceName] : 0;

  if (flags & kDNSServiceFlagsAdd) {
    scan->foundInterfaces[serviceName] = interfaces + 1;
    if (interfaces == 0) {
      if (events_.found) {
        events_.found(scan->scanId, serviceName);
      }
      if (!scan->typesOnly) {
        StartResolve(scan, serviceName, regtype, domain);
      }
    }
    return;
  }

  if (interfaces > 1) {
    scan->foundInterfaces[serviceName] = interfaces - 1;
    return;
  }
  scan->foundInterfaces.erase(serviceName);

  // Stop any pending resolve for the removed service
  auto resolving = scan->resolving.find(serviceName);
  if (resolving != scan->resolving.end()) {
    CancelResolve(resolving->second);
    scan->resolving.erase(resolving);
  }
  if (events_.remove) {
    events_.remove(scan->scanId, serviceName);
  }
}

// Resolve

void DnssdBackend::StartResolve(const std::shared_ptr<DnssdScan> &scan, const std::string &name, const std::string &regtype, const std::string &domain) {
  auto resolve = std::make_shared<DnssdResolve>();
  resolve->owner = weak_from_this();
  resolve->self = resolve;
  resolve->scan = scan;
  resolve->name = name;
  resolve->regtype = regtype;
  resolve->domain = domain;
  resolve->interfaceIndex = scan->interfaceIndex;
  resolve->timeoutSeconds = scan->resolveTimeoutSeconds;
  scan->resolving[name] = resolve;
  BeginResolve(resolve);
}

void DnssdBackend::BeginResolve(const std::shared_ptr<DnssdResolve> &resolve) {
  resolve->finished = false;
  resolve->addresses.clear();

  DNSServiceErrorType error = DNSServiceResolve(&resolve->resolveRef, 0, resolve->interfaceIndex, resolve->name.c_str(),
                                                resolve->regtype.c_str(), resolve->domain.c_str(), ResolveReply, resolve.get());
  if (error != kDNSServiceErr_NoError) {
    resolve->resolveRef = nullptr;
    FailResolve(resolve, DnssdError(error, "Resolving service", resolve->name));
    return;
  }
  executor_->Attach(resolve->resolveRef);

  std::weak_ptr<DnssdBackend> weakSelf = weak_from_this();
  std::weak_ptr<DnssdResolve> weakResolve = resolve;
  resolve->timeoutTimer = executor_->PostDelayed(resolve->timeoutSeconds, [weakSelf, weakResolve] {
    auto self = weakSelf.lock();
    auto timedOut = weakResolve.lock();
    if (self && timedOut) {
      timedOut->timeoutTimer = 0;
      self->ResolveTimedOut(timedOut);
    }
  });
}

void DnssdBackend::ResolveTimedOut(const std::shared_ptr<DnssdResolve> &resolve) {
  if (resolve->finished || resolve->emitted || !IsActive(resolve)) {
    return;
  }
  CancelResolve(resolve);

  // Slow devices can time out, retry once before reporting the error
  if (!resolve->retried) {
    resolve->retried = true;
    BeginResolve(resolve);
    return;
  }
  FailResolve(resolve, LibraryError("TIMEOUT", "Resolving service " + resolve->name + " failed: timed out", resolve->name));
}

void DnssdBackend::OnResolve(const std::shared_ptr<DnssdResolve> &resolve, DNSServiceErrorType error, const std::string &host, uint16_t port, const std::string &txt) {
  if (resolve->finished || resolve->resolveRef == nullptr) {
    return;
  }
  if (error != kDNSServiceErr_NoError) {
    FailResolve(resolve, DnssdError(error, "Resolving service", resolve->name));
    return;
  }

  // Only the first answer is needed
  DeallocateLater(resolve->resolveRef, resolve);

  resolve->host = host;
  resolve->port = port;
  resolve->txt = DecodeTxt(txt.data(), static_cast<uint16_t>(txt.size()));

  DNSServiceErrorType addressError = DNSServiceGetAddrInfo(&resolve->addressRef, 0, resolve->interfaceIndex,
                                                           kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6,
                                                           resolve->host.c_str(), AddressReply, resolve.get());
  if (addressError != kDNSServiceErr_NoError) {
    resolve->addressRef = nullptr;
    FailResolve(resolve, DnssdError(addressError, "Resolving service", resolve->name));
    return;
  }
  executor_->Attach(resolve->addressRef);
}

void DnssdBackend::OnAddress(const std::shared_ptr<DnssdResolve> &resolve, DNSServiceFlags flags, DNSServiceErrorType error, const std::string &string) {
  if (resolve->finished || resolve->addressRef == nullptr) {
    return;
  }
  if (error != kDNSServiceErr_NoError) {
    // No record for one of the address families is not a failure
    if (error == kDNSServiceErr_NoSuchRecord) {
      return;
    }
    FailResolve(resolve, DnssdError(error, "Resolving service", resolve->name));
    return;
  }

  auto existing = std::find(resolve->addresses.begin(), resolve->addresses.end(), string);
  if (!string.empty() && (flags & kDNSServiceFlagsAdd)) {
    if (existing == resolve->addresses.end()) {
      resolve->addresses.push_back(string);
    }
  } else if (!string.empty() && existing != resolve->addresses.end()) {
    resolve->addresses.erase(existing);
  }
  if (flags & kDNSServiceFlagsMoreComing) {
    return;
  }
  ScheduleEmit(resolve);
}

// Follows the TXT record of a resolved service
void DnssdBackend::WatchTxt(const std::shared_ptr<DnssdResolve> &resolve) {
  char fullName[kDNSServiceMaxDomainName];
  if (DNSServiceConstructFullName(fullName, resolve->name.c_str(), resolve->regtype.c_str(), resolve->domain.c_str()) != kDNSServiceErr_NoError) {
    return;
  }
  if (DNSServiceQueryRecord(&resolve->txtRef, 0, resolve->interfaceIndex, fullName, kDNSServiceType_TXT, kDNSServiceClass_IN,
                            TxtReply, resolve.get()) != kDNSServiceErr_NoError) {
    resolve->txtRef = nullptr;
    return;
  }
  executor_->Attach(resolve->txtRef);
}

void DnssdBackend::OnTxt(const std::shared_ptr<DnssdResolve> &resolve, DNSServiceFlags flags, DNSServiceErrorType error, const std::string &data) {
  if (resolve->finished || resolve->txtRef == nullptr || error != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd)) {
    return;
  }
  resolve->txt = DecodeTxt(data.data(), static_cast<uint16_t>(data.size()));
  ScheduleEmit(resolve);
}

// IPv4 and IPv6 answers can arrive in separate batches, wait briefly before emitting
void DnssdBackend::ScheduleEmit(const std::shared_ptr<DnssdResolve> &resolve) {
  if (resolve->emitTimer) {
    executor_->Cancel(resolve->emitTimer);
  }
  std::weak_ptr<DnssdBackend> weakSelf = weak_from_this();
  std::weak_ptr<DnssdResolve> weakResolve = resolve;
  resolve->emitTimer = executor_->PostDelayed(0.25, [weakSelf, weakResolve] {
    auto self = weakSelf.lock();
    auto emitting = weakResolve.lock();
    if (self && emitting) {
      emitting->emitTimer = 0;
      self->EmitResolve(emitting);
    }
  });
}

void DnssdBackend::EmitResolve(const std::shared_ptr<DnssdResolve> &resolve) {
  if (resolve->finished || resolve->addresses.empty() || !IsActive(resolve)) {
    return;
  }
  Service service;
  service.name = resolve->name;
  service.fullName = resolve->host + resolve->regtype + ".";
  service.host = resolve->host;
  service.port = resolve->port;
  service.addresses = resolve->addresses;
  service.txt = resolve->txt;

  if (resolve->single) {
    ServiceCallback promiseResolve = resolve->promiseResolve;
    EndSingleResolve(resolve);
    if (promiseResolve) {
      promiseResolve(service);
    }
    return;
  }

  if (!resolve->emitted) {
    resolve->emitted = true;
    if (resolve->timeoutTimer) {
      executor_->Cancel(resolve->timeoutTimer);
      resolve->timeoutTimer = 0;
    }
    WatchTxt(resolve);
  }
  // Emitted again only when something changed
  if (resolve->lastEmitted && *resolve->lastEmitted == service) {
    return;
  }
  resolve->lastEmitted = service;
  auto scan = resolve->scan.lock();
  if (scan && events_.resolved) {
    events_.resolved(scan->scanId, service);
  }
}

void DnssdBackend::FailResolve(const std::shared_ptr<DnssdResolve> &resolve, const Error &error) {
  if (resolve->single) {
    ErrorCallback promiseReject = resolve->promiseReject;
    EndSingleResolve(resolve);
    if (promiseReject) {
      promiseReject(error);
    }
    return;
  }
  CancelResolve(resolve);
  auto scan = resolve->scan.lock();
  if (!scan) {
    return;
  }
  auto entry = scan->resolving.find(resolve->name);
  if (entry != scan->resolving.end() && entry->second == resolve) {
    scan->resolving.erase(entry);
  }
  SendError(scan->scanId, error);
}

bool DnssdBackend::IsActive(const std::shared_ptr<DnssdResolve> &resolve) const {
  if (resolve->single) {
    return singleResolves_.count(resolve) > 0;
  }
  auto scan = resolve->scan.lock();
  if (!scan) {
    return false;
  }
  auto entry = scan->resolving.find(resolve->name);
  return entry != scan->resolving.end() && entry->second == resolve;
}

void DnssdBackend::EndSingleResolve(const std::shared_ptr<DnssdResolve> &resolve) {
  CancelResolve(resolve);
  resolve->promiseResolve = nullptr;
  resolve->promiseReject = nullptr;
  singleResolves_.erase(resolve);
}

// Stops everything in flight for a resolve, safe to call from its own callbacks
void DnssdBackend::CancelResolve(const std::shared_ptr<DnssdResolve> &resolve) {
  resolve->finished = true;
  if (resolve->timeoutTimer) {
    executor_->Cancel(resolve->timeoutTimer);
    resolve->timeoutTimer = 0;
  }
  if (resolve->emitTimer) {
    executor_->Cancel(resolve->emitTimer);
    resolve->emitTimer = 0;
  }
  DeallocateLater(resolve->resolveRef, resolve);
  DeallocateLater(resolve->addressRef, resolve);
  DeallocateLater(resolve->txtRef, resolve);
}

void DnssdBackend::ResolveService(
    const std::string &name,
    const std::string &type,
    const std::string &protocol,
    const std::string &domain,
    const ResolveOptions &options,
    ServiceCallback resolve,
    ErrorCallback reject) {
  Run([=](DnssdBackend &self) {
    auto single = std::make_shared<DnssdResolve>();
    if (!InterfaceIndex(options.networkInterface, single->interfaceIndex)) {
      reject(UnknownInterface(options.networkInterface));
      return;
    }
    single->owner = self.weak_from_this();
    single->self = single;
    single->single = true;
    single->name = name;
    single->regtype = "_" + type + "._" + protocol;
    single->domain = domain.empty() ? "local." : domain;
    single->timeoutSeconds = options.timeoutSeconds > 0 ? options.timeoutSeconds : 5;
    single->promiseResolve = resolve;
    single->promiseReject = reject;
    // The timeout is the caller's, no retry as for scans
    single->retried = true;
    self.singleResolves_.insert(single);
    self.BeginResolve(single);
  });
}

// Publish

void DnssdBackend::Publish(
    const std::string &type,
    const std::string &protocol,
    const std::string &domain,
    const std::string &requestedName,
    uint16_t port,
    const TxtPairs &txt,
    const PublishOptions &options,
    ServiceCallback resolve,
    ErrorCallback reject) {
  Run([=](DnssdBackend &self) {
    uint32_t interfaceIndex = kDNSServiceInterfaceIndexAny;
    if (!InterfaceIndex(options.networkInterface, interfaceIndex)) {
      Error error = UnknownInterface(options.networkInterface);
      self.SendError("", error);
      reject(error);
      return;
    }

    std::vector<std::string> tooLong;
    std::string record = EncodeTxt(txt, &tooLong);
    for (const auto &key : tooLong) {
      self.SendError("", LibraryError("TXT_ENTRY_TOO_LONG", "TXT record entry " + key + " is longer than 255 bytes", requestedName));
    }

    // dns_sd lets the same app register a name twice, rename like Bonjour does on conflicts
    std::string name = requestedName;
    for (int suffix = 2; self.IsNameInUse(name); suffix++) {
      name = requestedName + " (" + std::to_string(suffix) + ")";
    }

    auto publication = std::make_shared<DnssdPublication>();
    publication->owner = self.weak_from_this();
    publication->self = publication;
    publication->name = name;
    publication->regtype = "_" + type + "._" + protocol;
    publication->domain = domain.empty() ? "local." : domain;
    publication->port = port;
    publication->txt = DecodeTxt(record.data(), static_cast<uint16_t>(record.size()));
    publication->resolve = resolve;
    publication->reject = reject;

    // "_ipp._tcp,_printer,_color" registers the subtypes too
    std::string registerType = publication->regtype;
    for (const auto &subtype : options.subtypes) {
      std::string label = SubtypeLabel(subtype);
      if (!label.empty()) {
        registerType += "," + label;
      }
    }

    DNSServiceErrorType error = DNSServiceRegister(&publication->ref, 0, interfaceIndex, name.c_str(), registerType.c_str(),
                                                   domain.empty() ? nullptr : domain.c_str(), nullptr, htons(port),
                                                   static_cast<uint16_t>(record.size()), record.empty() ? nullptr : record.data(),
                                                   RegisterReply, publication.get());
    if (error != kDNSServiceErr_NoError) {
      publication->ref = nullptr;
      Error errorInfo = DnssdError(error, "Publishing service", name);
      self.SendError("", errorInfo);
      reject(errorInfo);
      return;
    }
    self.executor_->Attach(publication->ref);
    self.pending_.insert(publication);
  });
}

void DnssdBackend::OnRegister(const std::shared_ptr<DnssdPublication> &publication, DNSServiceFlags flags, DNSServiceErrorType error, const std::optional<std::string> &name) {
  if (publication->ref == nullptr) {
    return;
  }
  if (error != kDNSServiceErr_NoError) {
    Error errorInfo = DnssdError(error, "Publishing service", publication->name);
    SendError("", errorInfo);
    pending_.erase(publication);
    auto published = published_.find(publication->name);
    if (published != published_.end() && published->second == publication) {
      published_.erase(published);
    }
    DeallocateLater(publication->ref, publication);
    if (publication->reject) {
      publication->reject(errorInfo);
    }
    publication->resolve = nullptr;
    publication->reject = nullptr;
    return;
  }
  if (!(flags & kDNSServiceFlagsAdd)) {
    return;
  }

  // The name can differ from the requested one when it was already taken
  std::string registeredName = name ? *name : publication->name;
  auto previous = published_.find(publication->name);
  if (previous != published_.end() && previous->second == publication) {
    published_.erase(previous);
  }
  publication->name = registeredName;
  published_[registeredName] = publication;
  pending_.erase(publication);

  Service service = PublishedService(*publication);
  if (events_.published) {
    events_.published(service);
  }
  if (publication->resolve) {
    publication->resolve(service);
  }
  publication->resolve = nullptr;
  publication->reject = nullptr;
}

void DnssdBackend::Unpublish(const std::string &name, ServiceCallback resolve, ErrorCallback reject) {
  Run([=](DnssdBackend &self) {
    auto found = self.published_.find(name);
    if (found == self.published_.end()) {
      reject(NotPublished(name));
      return;
    }
    auto publication = found->second;
    self.published_.erase(found);
    // Deallocating the ref unregisters the service
    DNSServiceRefDeallocate(publication->ref);
    publication->ref = nullptr;

    Service service = self.PublishedService(*publication);
    if (self.events_.unpublished) {
      self.events_.unpublished(service);
    }
    resolve(service);
  });
}

// Replaces the TXT record of a published service
void DnssdBackend::Update(const std::string &name, const TxtPairs &txt, ServiceCallback resolve, ErrorCallback reject) {
  Run([=](DnssdBackend &self) {
    auto found = self.published_.find(name);
    if (found == self.published_.end()) {
      reject(NotPublished(name));
      return;
    }
    auto publication = found->second;

    std::vector<std::string> tooLong;
    std::string record = EncodeTxt(txt, &tooLong);
    for (const auto &key : tooLong) {
      self.SendError("", LibraryError("TXT_ENTRY_TOO_LONG", "TXT record entry " + key + " is longer than 255 bytes", name));
    }
    // An empty TXT record is a single empty string
    static const char emptyTxt = 0;
    DNSServiceErrorType error = DNSServiceUpdateRecord(publication->ref, nullptr, 0,
                                                       record.empty() ? 1 : static_cast<uint16_t>(record.size()),
                                                       record.empty() ? &emptyTxt : record.data(), 0);
    if (error != kDNSServiceErr_NoError) {
      reject(DnssdError(error, "Updating service", name));
      return;
    }
    publication->txt = DecodeTxt(record.data(), static_cast<uint16_t>(record.size()));
    resolve(self.PublishedService(*publication));
  });
}

bool DnssdBackend::IsNameInUse(const std::string &name) const {
  if (published_.count(name)) {
    return true;
  }
  for (const auto &publication : pending_) {
    if (publication->name == name) {
      return true;
    }
  }
  return false;
}

Service DnssdBackend::PublishedService(const DnssdPublication &publication) const {
  Service service;
  service.name = publication.name;
  service.fullName = publication.name + "." + publication.regtype + "." + publication.domain;
  service.port = publication.port;
  service.txt = publication.txt;
  return service;
}

// Teardown

void DnssdBackend::Shutdown() {
  // Keeps the backend alive until the cleanup ran
  auto self = shared_from_this();
  executor_->Post([self] {
    std::vector<std::string> ids;
    for (const auto &entry : self->scans_) {
      ids.push_back(entry.first);
    }
    for (const auto &id : ids) {
      self->StopScan(id);
    }
    for (auto &entry : self->published_) {
      if (entry.second->ref) {
        DNSServiceRefDeallocate(entry.second->ref);
        entry.second->ref = nullptr;
      }
    }
    self->published_.clear();
    for (const auto &publication : self->pending_) {
      if (publication->ref) {
        DNSServiceRefDeallocate(publication->ref);
        publication->ref = nullptr;
      }
    }
    self->pending_.clear();
    for (const auto &single : self->singleResolves_) {
      self->CancelResolve(single);
    }
    self->singleResolves_.clear();
  });
}

} // namespace rnzeroconf
