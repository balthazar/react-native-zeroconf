#include "ZeroconfCore.h"

#include <iphlpapi.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <thread>

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace rnzeroconf::win {

using After = std::vector<std::function<void()>>;

enum class Kind { Browse, Resolve, Publication };

struct Operation {
  Operation(Kind kind, Zeroconf *owner) : kind(kind), owner(owner) {}
  virtual ~Operation() = default;
  const Kind kind;
  Zeroconf *owner;
};

struct Resolve : Operation {
  explicit Resolve(Zeroconf *owner) : Operation(Kind::Resolve, owner) {}
  // Empty for resolveService()
  std::weak_ptr<Browse> browse;
  std::string scanId;
  std::wstring name;
  std::wstring key;
  std::wstring queryName;
  ULONG interfaceIndex = 0;
  DNS_SERVICE_RESOLVE_REQUEST request{};
  DNS_SERVICE_CANCEL cancel{};
  bool done = false;
  // Scans: retried once after a timeout
  bool retried = false;
  ServiceCallback promiseResolve;
  ErrorCallback promiseReject;
};

struct Browse : Operation {
  explicit Browse(Zeroconf *owner) : Operation(Kind::Browse, owner) {}
  std::string scanId;
  std::wstring regType;
  std::wstring domain;
  std::wstring queryName;
  bool typesOnly = false;
  ULONG interfaceIndex = 0;
  DWORD resolveTimeoutMs = 5000;
  DNS_SERVICE_BROWSE_REQUEST request{};
  DNS_SERVICE_CANCEL cancel{};
  // Lower-case PTR target -> name reported to JavaScript
  std::map<std::wstring, std::wstring> found;
  std::map<std::wstring, std::shared_ptr<Resolve>> resolves;
  // Service type scans: the types answered by the network, the others come from this app's publications
  std::set<std::wstring> networkTypes;
  // Instances resolved once, and their last TXT and SRV data, to resolve them again when they change
  std::set<std::wstring> resolved;
  std::map<std::wstring, std::wstring> recordData;
};

struct Publication : Operation {
  enum class State { Registering, Registered, Deregistering, Done };
  explicit Publication(Zeroconf *owner) : Operation(Kind::Publication, owner) {}
  ~Publication() override {
    if (instance) {
      DnsServiceFreeInstance(instance);
    }
  }
  State state = State::Registering;
  std::wstring requestedName;
  std::wstring name;
  std::wstring regType;
  std::wstring domain;
  uint16_t port = 0;
  TxtPairs txt;
  ULONG interfaceIndex = 0;
  // Updates publish again without emitting unpublished and published
  bool announce = true;
  PDNS_SERVICE_INSTANCE instance = nullptr;
  DNS_SERVICE_REGISTER_REQUEST request{};
  DNS_SERVICE_CANCEL cancel{};
  ServiceCallback onRegistered;
  ErrorCallback onRegisterFailed;
  ServiceCallback onDeregistered;
};

namespace {

// windns.h callbacks receive a context pointer, they only use operations still in this registry,
// so a callback arriving after a cancel never touches a freed operation
std::mutex registryMutex;
std::map<void *, std::shared_ptr<Operation>> &Registry() {
  static std::map<void *, std::shared_ptr<Operation>> registry;
  return registry;
}

void Track(const std::shared_ptr<Operation> &operation) {
  std::lock_guard<std::mutex> lock(registryMutex);
  Registry()[operation.get()] = operation;
}

void Untrack(Operation *operation) {
  std::lock_guard<std::mutex> lock(registryMutex);
  Registry().erase(operation);
}

template <typename T>
std::shared_ptr<T> Tracked(void *context) {
  std::lock_guard<std::mutex> lock(registryMutex);
  auto it = Registry().find(context);
  if (it == Registry().end() || it->second->owner == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<T>(it->second);
}

bool DebugEnabled() {
  static const bool enabled = GetEnvironmentVariableW(L"RNZEROCONF_DEBUG", nullptr, 0) > 0;
  return enabled;
}

std::wstring Lower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
  return value;
}

bool EndsWith(const std::wstring &value, const std::wstring &suffix) {
  return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::wstring WithoutTrailingDot(std::wstring value) {
  while (!value.empty() && value.back() == L'.') {
    value.pop_back();
  }
  return value;
}

// "local." -> "local"
std::wstring DomainName(const std::wstring &domain) {
  std::wstring name = WithoutTrailingDot(domain);
  return name.empty() ? L"local" : name;
}

// "printer" or "_printer" -> "_printer"
std::wstring Label(const std::wstring &value) {
  return value.empty() || value[0] == L'_' ? value : L"_" + value;
}

// "My Printer._ipp._tcp.local" -> "My Printer"
std::wstring InstanceName(const std::wstring &fullName, const std::wstring &regType, const std::wstring &domain) {
  std::wstring name = WithoutTrailingDot(fullName);
  std::wstring suffix = Lower(L"." + regType + L"." + domain);
  if (EndsWith(Lower(name), suffix)) {
    return name.substr(0, name.size() - suffix.size());
  }
  size_t typeStart = name.find(L"._");
  return typeStart == std::wstring::npos ? name : name.substr(0, typeStart);
}

// TXT strings or SRV target and port, to notice changes
std::wstring RecordData(const DNS_RECORD &record) {
  std::wstring data;
  if (record.wType == DNS_TYPE_TEXT) {
    for (DWORD i = 0; i < record.Data.TXT.dwStringCount; i++) {
      data += std::wstring(record.Data.TXT.pStringArray[i] ? record.Data.TXT.pStringArray[i] : L"") + L'\n';
    }
  } else if (record.wType == DNS_TYPE_SRV) {
    data = std::wstring(record.Data.SRV.pNameTarget ? record.Data.SRV.pNameTarget : L"") + L":" + std::to_wstring(record.Data.SRV.wPort);
  }
  return data;
}

// "_http._tcp.local" -> "_http._tcp"
std::wstring ServiceTypeName(const std::wstring &target) {
  size_t first = target.find(L'.');
  if (first == std::wstring::npos) {
    return target;
  }
  size_t second = target.find(L'.', first + 1);
  return second == std::wstring::npos ? target : target.substr(0, second);
}

Error WindowsError(DWORD status, const std::wstring &prefix, const std::wstring &serviceName) {
  Error error;
  error.domain = L"Windows";
  error.code = static_cast<int64_t>(status);
  error.message = prefix + DescribeStatus(status);
  error.serviceName = serviceName;
  return error;
}

Error LibraryError(const std::wstring &code, const std::wstring &message, const std::wstring &serviceName) {
  Error error;
  error.domain = L"RNZeroconf";
  error.stringCode = code;
  error.message = message;
  error.serviceName = serviceName;
  return error;
}

std::wstring AddressString(int family, const void *address) {
  wchar_t buffer[INET6_ADDRSTRLEN] = {};
  return InetNtopW(family, address, buffer, INET6_ADDRSTRLEN) ? std::wstring(buffer) : std::wstring();
}

void Run(After &after) {
  for (auto &action : after) {
    action();
  }
  after.clear();
}

VOID WINAPI BrowseCallback(DWORD status, PVOID context, PDNS_RECORD records) {
  auto browse = Tracked<Browse>(context);
  if (!browse) {
    if (records) {
      DnsRecordListFree(records, DnsFreeRecordList);
    }
    return;
  }
  browse->owner->OnBrowse(browse, status, records);
}

VOID WINAPI ResolveCallback(DWORD status, PVOID context, PDNS_SERVICE_INSTANCE instance) {
  auto resolve = Tracked<Resolve>(context);
  if (!resolve) {
    if (instance) {
      DnsServiceFreeInstance(instance);
    }
    return;
  }
  resolve->owner->OnResolve(resolve, status, instance);
}

VOID WINAPI RegisterCallback(DWORD status, PVOID context, PDNS_SERVICE_INSTANCE instance) {
  auto publication = Tracked<Publication>(context);
  if (!publication) {
    if (instance) {
      DnsServiceFreeInstance(instance);
    }
    return;
  }
  publication->owner->OnRegister(publication, status, instance);
}

} // namespace

std::wstring Widen(const std::string &value) {
  if (value.empty()) {
    return std::wstring();
  }
  int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  std::wstring result(size, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], size);
  return result;
}

std::string Narrow(const std::wstring &value) {
  if (value.empty()) {
    return std::string();
  }
  int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], size, nullptr, nullptr);
  return result;
}

std::wstring DescribeStatus(DWORD status) {
  wchar_t *buffer = nullptr;
  DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      status,
      0,
      reinterpret_cast<LPWSTR>(&buffer),
      0,
      nullptr);
  std::wstring message = length && buffer ? std::wstring(buffer, length) : L"error " + std::to_wstring(status);
  if (buffer) {
    LocalFree(buffer);
  }
  while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r' || message.back() == L'.' || message.back() == L' ')) {
    message.pop_back();
  }
  return message;
}

Zeroconf::Zeroconf(Events events) : events_(std::move(events)), alive_(std::make_shared<std::atomic<bool>>(true)) {
  wchar_t name[256] = {};
  DWORD size = 256;
  if (GetComputerNameExW(ComputerNameDnsHostname, name, &size) && size > 0) {
    hostName_ = std::wstring(name, size) + L".local";
  } else {
    hostName_ = L"windows.local";
  }
}

Zeroconf::~Zeroconf() {
  *alive_ = false;
  StopAll();
  UnpublishAll();
  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto &resolve : singleResolves_) {
      CancelResolve(resolve, after);
    }
    singleResolves_.clear();
  }
  Run(after);
  // Late callbacks of deregistrations still in flight are ignored
  std::lock_guard<std::mutex> lock(registryMutex);
  for (auto &entry : Registry()) {
    if (entry.second->owner == this) {
      entry.second->owner = nullptr;
    }
  }
}

bool Zeroconf::InterfaceIndex(const std::wstring &networkInterface, ULONG &index, Error &error) const {
  index = 0;
  if (networkInterface.empty()) {
    return true;
  }
  if (std::all_of(networkInterface.begin(), networkInterface.end(), [](wchar_t c) { return std::iswdigit(c); })) {
    index = std::wcstoul(networkInterface.c_str(), nullptr, 10);
    return true;
  }
  // By friendly name ("Wi-Fi", "Ethernet") or adapter name
  ULONG size = 16 * 1024;
  std::vector<BYTE> buffer(size);
  ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
  if (result == ERROR_BUFFER_OVERFLOW) {
    buffer.resize(size);
    result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
  }
  if (result == NO_ERROR) {
    std::wstring wanted = Lower(networkInterface);
    for (auto adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()); adapter; adapter = adapter->Next) {
      bool friendly = adapter->FriendlyName && Lower(adapter->FriendlyName) == wanted;
      bool adapterName = adapter->AdapterName && Lower(Widen(adapter->AdapterName)) == wanted;
      if (friendly || adapterName) {
        index = adapter->IfIndex ? adapter->IfIndex : adapter->Ipv6IfIndex;
        return true;
      }
    }
  }
  error = LibraryError(L"UNKNOWN_INTERFACE", L"Unknown network interface " + networkInterface, L"");
  return false;
}

// Scan

void Zeroconf::Scan(
    const std::string &scanId,
    const std::wstring &type,
    const std::wstring &protocol,
    const std::wstring &domain,
    const std::wstring &subtype,
    const std::wstring &networkInterface,
    double resolveTimeoutSeconds) {
  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    StopLocked(scanId, after);

    auto browse = std::make_shared<Browse>(this);
    browse->scanId = scanId;
    browse->resolveTimeoutMs = static_cast<DWORD>((resolveTimeoutSeconds > 0 ? resolveTimeoutSeconds : 5) * 1000);
    browse->regType = L"_" + type + L"._" + protocol;
    browse->domain = DomainName(domain);
    browse->typesOnly = type == L"services._dns-sd" && protocol == L"udp";
    Error interfaceError;
    if (!InterfaceIndex(networkInterface, browse->interfaceIndex, interfaceError)) {
      after.push_back([this, scanId, interfaceError] {
        if (events_.error) events_.error(scanId, interfaceError);
      });
    } else {
      // "_printer._sub._ipp._tcp.local" browses the _printer subtype
      browse->queryName = (subtype.empty() ? L"" : Label(subtype) + L"._sub.") + browse->regType + L"." + browse->domain;
      browse->request.Version = DNS_QUERY_REQUEST_VERSION1;
      browse->request.InterfaceIndex = browse->interfaceIndex;
      browse->request.QueryName = browse->queryName.c_str();
      browse->request.pBrowseCallback = BrowseCallback;
      browse->request.pQueryContext = browse.get();
      Track(browse);
      DNS_STATUS status = DnsServiceBrowse(&browse->request, &browse->cancel);
      if (status != DNS_REQUEST_PENDING) {
        Untrack(browse.get());
        Error error = WindowsError(status, L"Browsing services failed: ", L"");
        after.push_back([this, scanId, error] {
          if (events_.error) events_.error(scanId, error);
        });
      } else {
        browses_[scanId] = browse;
        after.push_back([this, scanId] {
          if (events_.start) events_.start(scanId);
        });
        if (browse->typesOnly) {
          for (auto &entry : publications_) {
            SyncLocalType(entry.second->regType, entry.second->domain, after);
          }
        }
      }
    }
  }
  Run(after);
}

void Zeroconf::Stop(const std::string &scanId) {
  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    StopLocked(scanId, after);
  }
  Run(after);
}

void Zeroconf::StopAll() {
  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::string> scanIds;
    for (auto &entry : browses_) {
      scanIds.push_back(entry.first);
    }
    for (auto &scanId : scanIds) {
      StopLocked(scanId, after);
    }
  }
  Run(after);
}

void Zeroconf::StopLocked(const std::string &scanId, After &after) {
  auto it = browses_.find(scanId);
  if (it == browses_.end()) {
    return;
  }
  auto browse = it->second;
  browses_.erase(it);
  Untrack(browse.get());
  for (auto &entry : browse->resolves) {
    CancelResolve(entry.second, after);
  }
  browse->resolves.clear();
  // Cancelling outside the lock, a callback in flight may be waiting for it
  after.push_back([this, browse, scanId] {
    DnsServiceBrowseCancel(&browse->cancel);
    if (events_.stop) events_.stop(scanId);
  });
}

void Zeroconf::OnBrowse(const std::shared_ptr<Browse> &browse, DWORD status, PDNS_RECORD records) {
  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto current = browses_.find(browse->scanId);
    if (current != browses_.end() && current->second == browse) {
      if (status != ERROR_SUCCESS && status != ERROR_CANCELLED) {
        Error error = WindowsError(status, L"Browsing services failed: ", L"");
        std::string scanId = browse->scanId;
        after.push_back([this, scanId, error] {
          if (events_.error) events_.error(scanId, error);
        });
      }
      for (PDNS_RECORD record = records; record; record = record->pNext) {
        if (DebugEnabled()) {
          fwprintf(stderr, L"[browse %hs] status=%lu type=%u ttl=%lu name=%ls target=%ls\n", browse->scanId.c_str(), status,
                   record->wType, record->dwTtl, record->pName ? record->pName : L"",
                   record->wType == DNS_TYPE_PTR && record->Data.PTR.pNameHost ? record->Data.PTR.pNameHost : L"");
        }
        // Live updates: a resolved service whose TXT or SRV record changes is resolved again
        if (!browse->typesOnly && (record->wType == DNS_TYPE_TEXT || record->wType == DNS_TYPE_SRV) && record->pName && record->dwTtl > 0) {
          std::wstring instanceKey = Lower(WithoutTrailingDot(record->pName));
          std::wstring dataKey = instanceKey + (record->wType == DNS_TYPE_TEXT ? L"#txt" : L"#srv");
          std::wstring data = RecordData(*record);
          auto previous = browse->recordData.find(dataKey);
          bool changed = previous != browse->recordData.end() && previous->second != data;
          browse->recordData[dataKey] = data;
          auto foundInstance = browse->found.find(instanceKey);
          if (changed && foundInstance != browse->found.end() && browse->resolved.count(instanceKey) && !browse->resolves.count(instanceKey)) {
            auto resolve = std::make_shared<Resolve>(this);
            resolve->browse = browse;
            resolve->scanId = browse->scanId;
            resolve->name = foundInstance->second;
            resolve->key = instanceKey;
            resolve->queryName = WithoutTrailingDot(record->pName);
            resolve->interfaceIndex = browse->interfaceIndex;
            browse->resolves[instanceKey] = resolve;
            StartScanResolve(browse, resolve, after);
          }
          continue;
        }
        if (record->wType != DNS_TYPE_PTR || !record->Data.PTR.pNameHost) {
          continue;
        }
        std::wstring target = WithoutTrailingDot(record->Data.PTR.pNameHost);
        std::wstring key = Lower(target);
        auto found = browse->found.find(key);
        std::string scanId = browse->scanId;

        // A zero TTL is a goodbye: the service left
        if (record->dwTtl == 0) {
          if (browse->typesOnly) {
            browse->networkTypes.erase(key);
            // Still listed while this app publishes it
            if (LocallyPublished(key)) {
              continue;
            }
          }
          if (found != browse->found.end()) {
            std::wstring name = found->second;
            browse->found.erase(found);
            auto resolve = browse->resolves.find(key);
            if (resolve != browse->resolves.end()) {
              CancelResolve(resolve->second, after);
              browse->resolves.erase(resolve);
            }
            browse->resolved.erase(key);
            after.push_back([this, scanId, name] {
              if (events_.remove) events_.remove(scanId, name);
            });
          }
          continue;
        }
        if (browse->typesOnly) {
          browse->networkTypes.insert(key);
        }
        if (found != browse->found.end()) {
          continue;
        }

        std::wstring name = browse->typesOnly ? ServiceTypeName(target) : InstanceName(target, browse->regType, browse->domain);
        // Service type answers can also announce subtypes ("_printer._sub._ipp._tcp"), only types are listed
        if (browse->typesOnly && !EndsWith(Lower(name), L"._tcp") && !EndsWith(Lower(name), L"._udp")) {
          continue;
        }
        browse->found[key] = name;
        after.push_back([this, scanId, name] {
          if (events_.found) events_.found(scanId, name);
        });
        // Service types are not resolved
        if (!browse->typesOnly) {
          auto resolve = std::make_shared<Resolve>(this);
          resolve->browse = browse;
          resolve->scanId = scanId;
          resolve->name = name;
          resolve->key = key;
          resolve->queryName = target;
          resolve->interfaceIndex = browse->interfaceIndex;
          browse->resolves[key] = resolve;
          StartScanResolve(browse, resolve, after);
        }
      }
    }
  }
  if (records) {
    DnsRecordListFree(records, DnsFreeRecordList);
  }
  Run(after);
}

// Resolve

void Zeroconf::StartResolve(const std::shared_ptr<Resolve> &resolve, After &after) {
  resolve->request.Version = DNS_QUERY_REQUEST_VERSION1;
  resolve->request.InterfaceIndex = resolve->interfaceIndex;
  resolve->request.QueryName = &resolve->queryName[0];
  resolve->request.pResolveCompletionCallback = ResolveCallback;
  resolve->request.pQueryContext = resolve.get();
  Track(resolve);
  DNS_STATUS status = DnsServiceResolve(&resolve->request, &resolve->cancel);
  if (status == DNS_REQUEST_PENDING) {
    return;
  }
  Untrack(resolve.get());
  resolve->done = true;
  Error error = WindowsError(status, L"Resolving service " + resolve->name + L" failed: ", resolve->name);
  if (resolve->promiseReject) {
    auto reject = resolve->promiseReject;
    after.push_back([reject, error] { reject(error); });
    return;
  }
  std::string scanId = resolve->scanId;
  after.push_back([this, scanId, error] {
    if (events_.error) events_.error(scanId, error);
  });
}

// Resolves of a scan time out after the scan's resolveTimeout, are retried once, then reported with a TIMEOUT error
void Zeroconf::StartScanResolve(const std::shared_ptr<Browse> &browse, const std::shared_ptr<Resolve> &resolve, After &after) {
  StartResolve(resolve, after);
  if (resolve->done) {
    return;
  }
  std::weak_ptr<Resolve> weak = resolve;
  auto alive = alive_;
  DWORD timeout = browse->resolveTimeoutMs;
  std::thread([this, weak, alive, timeout] {
    Sleep(timeout);
    if (!*alive) return;
    auto expired = weak.lock();
    if (!expired) return;
    After timedOut;
    {
      std::lock_guard<std::recursive_mutex> lock(mutex_);
      auto browse = expired->browse.lock();
      auto current = browse ? browses_.find(browse->scanId) : browses_.end();
      if (expired->done || current == browses_.end() || current->second != browse) return;
      auto entry = browse->resolves.find(expired->key);
      if (entry == browse->resolves.end() || entry->second != expired) return;
      CancelResolve(expired, timedOut);
      browse->resolves.erase(entry);
      if (!expired->retried) {
        // Slow devices can time out, retry once before reporting the error
        auto retry = std::make_shared<Resolve>(this);
        retry->browse = browse;
        retry->scanId = expired->scanId;
        retry->name = expired->name;
        retry->key = expired->key;
        retry->queryName = expired->queryName;
        retry->interfaceIndex = expired->interfaceIndex;
        retry->retried = true;
        browse->resolves[retry->key] = retry;
        StartScanResolve(browse, retry, timedOut);
      } else {
        Error error = LibraryError(L"TIMEOUT", L"Resolving service " + expired->name + L" failed: timed out", expired->name);
        std::string scanId = expired->scanId;
        timedOut.push_back([this, scanId, error] {
          if (events_.error) events_.error(scanId, error);
        });
      }
    }
    Run(timedOut);
  }).detach();
}

void Zeroconf::CancelResolve(const std::shared_ptr<Resolve> &resolve, After &after) {
  if (resolve->done) {
    return;
  }
  resolve->done = true;
  Untrack(resolve.get());
  after.push_back([resolve] { DnsServiceResolveCancel(&resolve->cancel); });
}

void Zeroconf::OnResolve(const std::shared_ptr<Resolve> &resolve, DWORD status, PDNS_SERVICE_INSTANCE instance) {
  Service service;
  bool resolved = status == ERROR_SUCCESS && instance != nullptr;
  if (resolved) {
    service.name = resolve->name;
    service.fullName = instance->pszInstanceName ? instance->pszInstanceName : resolve->queryName;
    service.host = instance->pszHostName ? instance->pszHostName : L"";
    service.port = instance->wPort;
    if (instance->ip4Address) {
      IN_ADDR address{};
      address.S_un.S_addr = *instance->ip4Address;
      std::wstring text = AddressString(AF_INET, &address);
      if (!text.empty()) service.addresses.push_back(text);
    }
    if (instance->ip6Address) {
      IN6_ADDR address{};
      memcpy(&address, instance->ip6Address->IP6Byte, sizeof(address));
      std::wstring text = AddressString(AF_INET6, &address);
      if (!text.empty()) service.addresses.push_back(text);
    }
    for (DWORD i = 0; i < instance->dwPropertyCount; i++) {
      if (instance->keys && instance->keys[i]) {
        service.txt.emplace_back(instance->keys[i], instance->values && instance->values[i] ? instance->values[i] : L"");
      }
    }
  }
  if (DebugEnabled()) {
    fwprintf(stderr, L"[resolve %ls] status=%lu host=%ls port=%u addresses=%zu txt=%zu\n", resolve->name.c_str(), status,
             service.host.c_str(), service.port, service.addresses.size(), service.txt.size());
  }
  if (instance) {
    DnsServiceFreeInstance(instance);
  }

  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Untrack(resolve.get());
    if (resolve->done) {
      return;
    }
    resolve->done = true;
    Error error = WindowsError(status, L"Resolving service " + resolve->name + L" failed: ", resolve->name);

    if (resolve->promiseResolve) {
      singleResolves_.erase(std::remove(singleResolves_.begin(), singleResolves_.end(), resolve), singleResolves_.end());
      auto promiseResolve = resolve->promiseResolve;
      auto promiseReject = resolve->promiseReject;
      after.push_back([resolved, service, error, promiseResolve, promiseReject] {
        if (resolved) {
          promiseResolve(service);
        } else {
          promiseReject(error);
        }
      });
    } else {
      auto browse = resolve->browse.lock();
      auto current = browse ? browses_.find(browse->scanId) : browses_.end();
      if (current == browses_.end() || current->second != browse) {
        return;
      }
      auto entry = browse->resolves.find(resolve->key);
      if (entry == browse->resolves.end() || entry->second != resolve) {
        return;
      }
      browse->resolves.erase(entry);
      if (resolved) {
        browse->resolved.insert(resolve->key);
      }
      std::string scanId = resolve->scanId;
      after.push_back([this, resolved, service, error, scanId] {
        if (resolved) {
          if (events_.resolved) events_.resolved(scanId, service);
        } else if (events_.error) {
          events_.error(scanId, error);
        }
      });
    }
  }
  Run(after);
}

void Zeroconf::ResolveService(
    const std::wstring &name,
    const std::wstring &type,
    const std::wstring &protocol,
    const std::wstring &domain,
    const std::wstring &networkInterface,
    double timeoutSeconds,
    ServiceCallback resolve,
    ErrorCallback reject) {
  After after;
  std::shared_ptr<Resolve> single;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    single = std::make_shared<Resolve>(this);
    Error interfaceError;
    if (!InterfaceIndex(networkInterface, single->interfaceIndex, interfaceError)) {
      after.push_back([reject, interfaceError] { reject(interfaceError); });
      single = nullptr;
    } else {
      single->name = name;
      single->queryName = name + L"._" + type + L"._" + protocol + L"." + DomainName(domain);
      single->promiseResolve = std::move(resolve);
      single->promiseReject = std::move(reject);
      singleResolves_.push_back(single);
      StartResolve(single, after);
    }
  }
  Run(after);
  if (!single) {
    return;
  }

  std::weak_ptr<Resolve> weak = single;
  auto alive = alive_;
  DWORD timeout = static_cast<DWORD>((timeoutSeconds > 0 ? timeoutSeconds : 5) * 1000);
  std::thread([this, weak, alive, timeout] {
    Sleep(timeout);
    if (!*alive) return;
    auto expired = weak.lock();
    if (!expired) return;
    After timedOut;
    {
      std::lock_guard<std::recursive_mutex> lock(mutex_);
      if (expired->done) return;
      CancelResolve(expired, timedOut);
      singleResolves_.erase(std::remove(singleResolves_.begin(), singleResolves_.end(), expired), singleResolves_.end());
      Error error = LibraryError(L"TIMEOUT", L"Resolving service " + expired->name + L" failed: timed out", expired->name);
      auto reject = expired->promiseReject;
      timedOut.push_back([reject, error] { reject(error); });
    }
    Run(timedOut);
  }).detach();
}

// Publish

void Zeroconf::Publish(
    const std::wstring &type,
    const std::wstring &protocol,
    const std::wstring &domain,
    const std::wstring &name,
    uint16_t port,
    const TxtPairs &txt,
    const std::wstring &networkInterface,
    ServiceCallback resolve,
    ErrorCallback reject) {
  ULONG interfaceIndex = 0;
  Error interfaceError;
  if (!InterfaceIndex(networkInterface, interfaceIndex, interfaceError)) {
    interfaceError.serviceName = name;
    if (events_.error) events_.error("", interfaceError);
    reject(interfaceError);
    return;
  }
  PublishAs(L"_" + type + L"._" + protocol, DomainName(domain), name, port, txt, interfaceIndex, true, std::move(resolve), std::move(reject));
}

void Zeroconf::PublishAs(
    const std::wstring &regType,
    const std::wstring &domain,
    const std::wstring &requestedName,
    uint16_t port,
    const TxtPairs &txt,
    ULONG interfaceIndex,
    bool announce,
    ServiceCallback resolve,
    ErrorCallback reject) {
  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // The same app can't publish a name twice, rename like Bonjour does on conflicts
    std::wstring name = requestedName;
    for (int suffix = 2; publications_.count(name); suffix++) {
      name = requestedName + L" (" + std::to_wstring(suffix) + L")";
    }

    auto publication = std::make_shared<Publication>(this);
    publication->requestedName = name;
    publication->name = name;
    publication->regType = regType;
    publication->domain = domain;
    publication->port = port;
    publication->txt = txt;
    publication->interfaceIndex = interfaceIndex;
    publication->announce = announce;
    publication->onRegistered = std::move(resolve);
    publication->onRegisterFailed = std::move(reject);

    std::vector<PCWSTR> keys;
    std::vector<PCWSTR> values;
    for (auto &pair : publication->txt) {
      keys.push_back(pair.first.c_str());
      values.push_back(pair.second.c_str());
    }
    std::wstring fullName = name + L"." + regType + L"." + domain;
    publication->instance = DnsServiceConstructInstance(
        fullName.c_str(),
        hostName_.c_str(),
        nullptr,
        nullptr,
        port,
        0,
        0,
        static_cast<DWORD>(keys.size()),
        keys.empty() ? nullptr : keys.data(),
        values.empty() ? nullptr : values.data());
    DNS_STATUS status = publication->instance ? DNS_REQUEST_PENDING : GetLastError();
    if (publication->instance) {
      publication->request.Version = DNS_QUERY_REQUEST_VERSION1;
      publication->request.InterfaceIndex = interfaceIndex;
      publication->request.pServiceInstance = publication->instance;
      publication->request.pRegisterCompletionCallback = RegisterCallback;
      publication->request.pQueryContext = publication.get();
      publication->request.hCredentials = nullptr;
      publication->request.unicastEnabled = FALSE;
      Track(publication);
      status = DnsServiceRegister(&publication->request, &publication->cancel);
    }
    if (status == DNS_REQUEST_PENDING) {
      publications_[name] = publication;
    } else {
      Untrack(publication.get());
      Error error = WindowsError(status, L"Publishing service " + name + L" failed: ", name);
      auto onFailed = publication->onRegisterFailed;
      after.push_back([this, error, onFailed] {
        if (events_.error) events_.error("", error);
        if (onFailed) onFailed(error);
      });
    }
  }
  Run(after);
}

void Zeroconf::OnRegister(const std::shared_ptr<Publication> &publication, DWORD status, PDNS_SERVICE_INSTANCE instance) {
  std::wstring registeredName = publication->name;
  if (instance && instance->pszInstanceName) {
    registeredName = InstanceName(instance->pszInstanceName, publication->regType, publication->domain);
  }
  if (DebugEnabled()) {
    fwprintf(stderr, L"[register %ls] status=%lu state=%d name=%ls\n", publication->requestedName.c_str(), status,
             static_cast<int>(publication->state), registeredName.c_str());
  }
  if (instance && instance != publication->instance) {
    DnsServiceFreeInstance(instance);
  }

  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (publication->state == Publication::State::Registering) {
      auto entry = publications_.find(publication->name);
      bool current = entry != publications_.end() && entry->second == publication;
      if (status != ERROR_SUCCESS) {
        Untrack(publication.get());
        publication->state = Publication::State::Done;
        if (current) publications_.erase(entry);
        Error error = WindowsError(status, L"Publishing service " + publication->name + L" failed: ", publication->name);
        auto onFailed = publication->onRegisterFailed;
        after.push_back([this, error, onFailed] {
          if (events_.error) events_.error("", error);
          if (onFailed) onFailed(error);
        });
      } else {
        publication->state = Publication::State::Registered;
        if (current && registeredName != publication->name) {
          publications_.erase(entry);
          publications_[registeredName] = publication;
        }
        publication->name = registeredName;
        SyncLocalType(publication->regType, publication->domain, after);
        Service service = PublishedService(*publication);
        auto onRegistered = publication->onRegistered;
        bool announce = publication->announce;
        after.push_back([this, service, onRegistered, announce] {
          if (announce && events_.published) events_.published(service);
          if (onRegistered) onRegistered(service);
        });
      }
      publication->onRegistered = nullptr;
      publication->onRegisterFailed = nullptr;
    } else if (publication->state == Publication::State::Deregistering) {
      publication->state = Publication::State::Done;
      Untrack(publication.get());
      // An update publishes it again right after, keep the type listed
      if (publication->announce) {
        SyncLocalType(publication->regType, publication->domain, after);
      }
      Service service = PublishedService(*publication);
      auto onDeregistered = publication->onDeregistered;
      bool announce = publication->announce;
      after.push_back([this, service, onDeregistered, announce] {
        if (announce && events_.unpublished) events_.unpublished(service);
        if (onDeregistered) onDeregistered(service);
      });
      publication->onDeregistered = nullptr;
    }
  }
  Run(after);
}

bool Zeroconf::LocallyPublished(const std::wstring &typeKey) const {
  for (auto &entry : publications_) {
    auto &publication = entry.second;
    if (publication->state == Publication::State::Registered && Lower(publication->regType + L"." + publication->domain) == typeKey) {
      return true;
    }
  }
  return false;
}

void Zeroconf::SyncLocalType(const std::wstring &regType, const std::wstring &domain, After &after) {
  std::wstring key = Lower(regType + L"." + domain);
  bool published = LocallyPublished(key);
  for (auto &entry : browses_) {
    auto browse = entry.second;
    if (!browse->typesOnly || Lower(browse->domain) != Lower(domain)) {
      continue;
    }
    std::string scanId = browse->scanId;
    auto listed = browse->found.find(key);
    if (published && listed == browse->found.end()) {
      browse->found[key] = regType;
      after.push_back([this, scanId, regType] {
        if (events_.found) events_.found(scanId, regType);
      });
    } else if (!published && listed != browse->found.end() && !browse->networkTypes.count(key)) {
      std::wstring name = listed->second;
      browse->found.erase(listed);
      after.push_back([this, scanId, name] {
        if (events_.remove) events_.remove(scanId, name);
      });
    }
  }
}

Service Zeroconf::PublishedService(const Publication &publication) const {
  Service service;
  service.name = publication.name;
  service.fullName = publication.name + L"." + publication.regType + L"." + publication.domain;
  service.host = hostName_;
  service.port = publication.port;
  service.txt = publication.txt;
  return service;
}

void Zeroconf::Unpublish(const std::wstring &name, ServiceCallback resolve, ErrorCallback reject) {
  UnpublishWith(name, true, std::move(resolve), std::move(reject));
}

void Zeroconf::UnpublishWith(const std::wstring &name, bool announce, ServiceCallback resolve, ErrorCallback reject) {
  After after;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto entry = publications_.find(name);
    if (entry == publications_.end()) {
      for (auto it = publications_.begin(); it != publications_.end(); ++it) {
        if (it->second->requestedName == name) {
          entry = it;
          break;
        }
      }
    }
    if (entry == publications_.end()) {
      Error error = LibraryError(L"NOT_PUBLISHED", L"Service " + name + L" is not published", name);
      after.push_back([reject, error] { reject(error); });
    } else {
      auto publication = entry->second;
      publications_.erase(entry);
      if (publication->state == Publication::State::Registering) {
        // Not registered yet: cancel the registration
        publication->state = Publication::State::Done;
        Untrack(publication.get());
        Error error = LibraryError(L"NOT_PUBLISHED", L"Service " + name + L" was unpublished before it was published", name);
        auto onFailed = publication->onRegisterFailed;
        Service service = PublishedService(*publication);
        after.push_back([publication, onFailed, error, resolve, service] {
          DnsServiceRegisterCancel(&publication->cancel);
          if (onFailed) onFailed(error);
          resolve(service);
        });
      } else {
        publication->state = Publication::State::Deregistering;
        publication->announce = announce;
        publication->onDeregistered = std::move(resolve);
        after.push_back([this, publication, reject] {
          DWORD status = DnsServiceDeRegister(&publication->request, nullptr);
          if (status != DNS_REQUEST_PENDING) {
            Untrack(publication.get());
            reject(WindowsError(status, L"Unpublishing service " + publication->name + L" failed: ", publication->name));
          }
        });
      }
    }
  }
  Run(after);
}

void Zeroconf::Update(const std::wstring &name, const TxtPairs &txt, ServiceCallback resolve, ErrorCallback reject) {
  std::shared_ptr<Publication> publication;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto entry = publications_.find(name);
    if (entry != publications_.end() && entry->second->state == Publication::State::Registered) {
      publication = entry->second;
    }
  }
  if (!publication) {
    reject(LibraryError(L"NOT_PUBLISHED", L"Service " + name + L" is not published", name));
    return;
  }
  // No update in windns.h: unpublish, then publish again under the same name
  std::wstring regType = publication->regType;
  std::wstring domain = publication->domain;
  std::wstring registeredName = publication->name;
  uint16_t port = publication->port;
  ULONG interfaceIndex = publication->interfaceIndex;
  UnpublishWith(
      name,
      false,
      [this, regType, domain, registeredName, port, txt, interfaceIndex, resolve, reject](const Service &) {
        PublishAs(regType, domain, registeredName, port, txt, interfaceIndex, false, resolve, reject);
      },
      reject);
}

void Zeroconf::UnpublishAll() {
  std::vector<std::wstring> names;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto &entry : publications_) {
      names.push_back(entry.first);
    }
  }
  for (auto &name : names) {
    Unpublish(name, [](const Service &) {}, [](const Error &) {});
  }
}

} // namespace rnzeroconf::win
