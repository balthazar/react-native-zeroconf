// Harness for the dns_sd backend (cpp/dnssd), run on macOS against the system mDNSResponder,
// publishing and browsing real services. Run with test/apple/run.sh (AddressSanitizer).
#include "../../cpp/apple/DispatchExecutor.h"
#include "../../cpp/dnssd/DnssdBackend.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace rnzeroconf;

namespace {

struct Event {
  std::string name;
  std::string scanId;
  std::string subject;
  Service service;
  Error error;

  Event(std::string name, std::string scanId = "", std::string subject = "")
      : name(std::move(name)), scanId(std::move(scanId)), subject(std::move(subject)) {}
};

std::mutex eventsMutex;
std::vector<Event> events;
int failures = 0;

void Add(Event event) {
  std::lock_guard<std::mutex> lock(eventsMutex);
  events.push_back(std::move(event));
}

void Clear() {
  std::lock_guard<std::mutex> lock(eventsMutex);
  events.clear();
}

std::vector<Event> Named(const std::string &name, const std::string &scanId = "") {
  std::lock_guard<std::mutex> lock(eventsMutex);
  std::vector<Event> matching;
  for (const auto &event : events) {
    if (event.name == name && (scanId.empty() || event.scanId == scanId)) {
      matching.push_back(event);
    }
  }
  return matching;
}

void Sleep(double seconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(seconds * 1000)));
}

bool WaitFor(const std::function<bool()> &done, double seconds) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(seconds * 1000));
  while (!done()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    Sleep(0.05);
  }
  return true;
}

void Check(bool ok, const std::string &what) {
  printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok) {
    failures++;
  }
}

std::string Txt(const TxtPairs &txt, const std::string &key) {
  for (const auto &[k, v] : txt) {
    if (k == key) {
      return v;
    }
  }
  return "";
}

std::string Join(const std::vector<std::string> &values) {
  std::string joined;
  for (const auto &value : values) {
    joined += (joined.empty() ? "" : ",") + value;
  }
  return joined;
}

// A promise settled from the backend's thread
struct Result {
  std::mutex mutex;
  std::optional<Service> service;
  std::optional<Error> error;

  ServiceCallback Resolve() {
    return [this](const Service &value) {
      std::lock_guard<std::mutex> lock(mutex);
      service = value;
    };
  }
  ErrorCallback Reject() {
    return [this](const Error &value) {
      std::lock_guard<std::mutex> lock(mutex);
      error = value;
    };
  }
  bool Settled() {
    std::lock_guard<std::mutex> lock(mutex);
    return service || error;
  }
  bool Wait(double seconds) {
    return WaitFor([this] { return Settled(); }, seconds);
  }
};

// A service that is announced (PTR record) but never answers: no SRV or TXT record behind it
struct Ghost {
  DNSServiceRef connection = nullptr;
  DNSRecordRef record = nullptr;

  explicit Ghost(const std::string &instance, const std::string &type) {
    if (DNSServiceCreateConnection(&connection) != kDNSServiceErr_NoError) {
      connection = nullptr;
      return;
    }
    // PTR rdata: the instance's full name, in DNS label format
    std::string fullName = instance + "." + type + ".local.";
    std::string rdata;
    size_t start = 0;
    for (size_t i = 0; i <= fullName.size(); i++) {
      if (i == fullName.size() || fullName[i] == '.') {
        if (i > start) {
          rdata += static_cast<char>(i - start);
          rdata += fullName.substr(start, i - start);
        }
        start = i + 1;
      }
    }
    rdata += '\0';
    std::string ptrName = type + ".local.";
    DNSServiceRegisterRecord(connection, &record, kDNSServiceFlagsShared, kDNSServiceInterfaceIndexLocalOnly, ptrName.c_str(),
                             kDNSServiceType_PTR, kDNSServiceClass_IN, static_cast<uint16_t>(rdata.size()), rdata.data(), 120,
                             [](DNSServiceRef, DNSRecordRef, DNSServiceFlags, DNSServiceErrorType, void *) {}, nullptr);
    std::thread([ref = connection] {
      // Processes the registration reply on its own thread
      DNSServiceProcessResult(ref);
    }).detach();
  }
  ~Ghost() {
    if (connection) {
      DNSServiceRefDeallocate(connection);
    }
  }
};

} // namespace

int main() {
  Events handlers;
  handlers.start = [](const std::string &scanId) { Add({"start", scanId}); };
  handlers.stop = [](const std::string &scanId) { Add({"stop", scanId}); };
  handlers.found = [](const std::string &scanId, const std::string &name) { Add({"found", scanId, name}); };
  handlers.remove = [](const std::string &scanId, const std::string &name) { Add({"remove", scanId, name}); };
  handlers.resolved = [](const std::string &scanId, const Service &service) {
    Event event{"resolved", scanId, service.name};
    event.service = service;
    Add(event);
  };
  handlers.published = [](const Service &service) { Add({"published", "", service.name}); };
  handlers.unpublished = [](const Service &service) { Add({"unpublished", "", service.name}); };
  handlers.error = [](const std::string &scanId, const Error &error) {
    Event event{"error", scanId, error.serviceName};
    event.error = error;
    Add(event);
  };

  std::weak_ptr<DnssdBackend> weakBackend;
  {
    auto executor = std::make_shared<DispatchExecutor>("rnzeroconf.harness");
    auto backend = DnssdBackend::Create(executor, handlers);
    weakBackend = backend;
    ScanOptions scanOptions;

    // Publish, with ordered TXT
    Result published;
    backend->Publish("zcdns", "tcp", "local.", "zc-dnssd", 45690, {{"b", "2"}, {"a", "1"}, {"u", "café"}}, {}, published.Resolve(), published.Reject());
    published.Wait(5);
    Check(published.service && published.service->name == "zc-dnssd", "publish resolves with the name (" + (published.service ? published.service->name : published.error ? published.error->message : "nothing") + ")");
    Check(published.service && Txt(published.service->txt, "u") == "café" && published.service->txt.at(0).first == "b", "publish keeps the TXT order");
    Check(Named("published").size() == 1, "published event emitted once");

    // Same name again: registered under another name
    Result duplicate;
    backend->Publish("zcdns", "tcp", "local.", "zc-dnssd", 45691, {}, {}, duplicate.Resolve(), duplicate.Reject());
    duplicate.Wait(5);
    std::string duplicateName = duplicate.service ? duplicate.service->name : "";
    Check(!duplicateName.empty() && duplicateName != "zc-dnssd", "duplicate name is renamed (" + duplicateName + ")");

    // Scan: found once per service, resolved with host, port, addresses and TXT
    Clear();
    backend->Scan("A", "zcdns", "tcp", "local.", scanOptions);
    WaitFor([] { return Named("resolved", "A").size() >= 2; }, 5);
    Sleep(1);
    std::vector<std::string> foundNames;
    for (const auto &event : Named("found", "A")) foundNames.push_back(event.subject);
    Check(Named("start", "A").size() == 1, "start emitted");
    Check(std::count(foundNames.begin(), foundNames.end(), "zc-dnssd") == 1, "found once per service (" + Join(foundNames) + ")");
    std::optional<Service> resolved;
    for (const auto &event : Named("resolved", "A")) {
      if (event.subject == "zc-dnssd") resolved = event.service;
    }
    Check(resolved.has_value(), "resolved emitted");
    Check(resolved && resolved->port == 45690, "resolved port");
    Check(resolved && resolved->host.size() > 7 && resolved->host.substr(resolved->host.size() - 7) == ".local.", "resolved host (" + (resolved ? resolved->host : "") + ")");
    Check(resolved && !resolved->addresses.empty(), "resolved addresses (" + (resolved ? Join(resolved->addresses) : "") + ")");
    Check(resolved && Txt(resolved->txt, "a") == "1" && Txt(resolved->txt, "u") == "café", "resolved TXT");

    // Unpublish the duplicate while scanning: remove emitted for it
    Result gone;
    backend->Unpublish(duplicateName, gone.Resolve(), gone.Reject());
    gone.Wait(5);
    Check(gone.service && gone.service->name == duplicateName, "unpublish resolves");
    bool removed = WaitFor([&] {
      for (const auto &event : Named("remove", "A")) {
        if (event.subject == duplicateName) return true;
      }
      return false;
    }, 5);
    Check(removed, "remove emitted for the unpublished service");

    // Unknown and bad publishes
    Result unknown;
    backend->Unpublish("never", unknown.Resolve(), unknown.Reject());
    unknown.Wait(5);
    Check(unknown.error && unknown.error->code == "NOT_PUBLISHED", "unpublish of an unknown service rejects NOT_PUBLISHED");
    Result bad;
    backend->Publish("zcdns", "nope", "local.", "zc-bad", 1, {}, {}, bad.Resolve(), bad.Reject());
    bad.Wait(5);
    Check(bad.error && bad.error->domain == "DNSSD", "bad publish rejects with a DNSSD error (" + (bad.error ? bad.error->code + " " + bad.error->message : "") + ")");

    // A service that never answers: 1s timeout, retried once, then one TIMEOUT error after about 2s
    {
      Ghost ghost("zc-ghost", "_zcghost._tcp");
      Clear();
      ScanOptions quick;
      quick.resolveTimeoutSeconds = 1;
      auto start = std::chrono::steady_clock::now();
      backend->Scan("G", "zcghost", "tcp", "local.", quick);
      WaitFor([] { return !Named("error", "G").empty(); }, 8);
      double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      Sleep(2.5);
      auto errors = Named("error", "G");
      Check(!Named("found", "G").empty(), "the never-answering service is found");
      Check(errors.size() == 1 && errors[0].error.code == "TIMEOUT" && elapsed > 1.8 && elapsed < 3.5,
            "resolve retried once, then one TIMEOUT error after " + std::to_string(elapsed).substr(0, 3) + "s");
      Check(Named("resolved", "G").empty(), "the never-answering service isn't resolved");
      backend->Stop("G");
    }

    // Stress: 300 scan cycles with random short waits and stops
    std::mt19937 random(42);
    for (int i = 0; i < 300; i++) {
      backend->Scan("A", "zcdns", "tcp", "local.", scanOptions);
      Sleep((random() % 20) / 1000.0);
      if (random() % 3 == 0) {
        backend->Stop("");
        Sleep((random() % 5) / 1000.0);
      }
    }
    Clear();
    backend->Scan("A", "zcdns", "tcp", "local.", scanOptions);
    Check(WaitFor([] { return !Named("resolved", "A").empty(); }, 5), "scan still resolves after 300 rapid scan/stop cycles");

    // Two scans at once, for different types, each event tagged with its scan
    Result other;
    backend->Publish("zcother", "tcp", "local.", "zc-other", 45692, {}, {}, other.Resolve(), other.Reject());
    other.Wait(5);
    backend->Stop("");
    WaitFor([] { return !Named("stop").empty(); }, 2);
    Clear();
    backend->Scan("A", "zcdns", "tcp", "local.", scanOptions);
    backend->Scan("B", "zcother", "tcp", "local.", scanOptions);
    WaitFor([] { return !Named("resolved", "A").empty() && !Named("resolved", "B").empty(); }, 5);
    Sleep(1);
    std::set<std::string> resolvedA, resolvedB;
    for (const auto &event : Named("resolved", "A")) resolvedA.insert(event.subject);
    for (const auto &event : Named("resolved", "B")) resolvedB.insert(event.subject);
    Check(resolvedA.count("zc-dnssd") && !resolvedA.count("zc-other"), "scan A resolves only its type");
    Check(resolvedB == std::set<std::string>{"zc-other"}, "scan B resolves only its type");
    Check(Named("start", "A").size() == 1 && Named("start", "B").size() == 1, "both scans emit start with their id");
    Clear();
    backend->Stop("A");
    WaitFor([] { return !Named("stop").empty(); }, 2);
    Check(Named("stop").size() == 1 && Named("stop", "A").size() == 1, "stopping A emits stop for A only");
    Result goneOther;
    backend->Unpublish("zc-other", goneOther.Resolve(), goneOther.Reject());
    WaitFor([] { return !Named("remove").empty(); }, 5);
    Check(Named("remove").size() == 1 && Named("remove", "B").size() == 1, "scan B still sees removals after A stopped");

    // Service types: _services._dns-sd._udp reports "_zcdns._tcp" once, without resolving
    Clear();
    backend->Scan("T", "services._dns-sd", "udp", "local.", scanOptions);
    auto zcTypeCount = [] {
      int count = 0;
      for (const auto &event : Named("found", "T")) count += event.subject == "_zcdns._tcp";
      return count;
    };
    WaitFor([&] { return zcTypeCount() > 0; }, 5);
    Sleep(1);
    auto types = Named("found", "T");
    Check(zcTypeCount() == 1, "service types scan finds _zcdns._tcp once (" + std::to_string(types.size()) + " types)");
    bool allTypes = true;
    for (const auto &event : types) {
      const auto &name = event.subject;
      bool isType = name.size() > 5 && name[0] == '_' && (name.substr(name.size() - 5) == "._tcp" || name.substr(name.size() - 5) == "._udp");
      if (!isType) printf("     not a service type: %s\n", name.c_str());
      allTypes = allTypes && isType;
    }
    Check(allTypes, "every found name is a service type");
    Check(Named("resolved", "T").empty(), "service types are not resolved");
    backend->Stop("T");

    // Live updates: a resolved service is emitted again when its TXT record changes
    Clear();
    backend->Scan("L", "zcdns", "tcp", "local.", scanOptions);
    auto dnssdResolved = [] {
      std::vector<Service> matching;
      for (const auto &event : Named("resolved", "L")) {
        if (event.subject == "zc-dnssd") matching.push_back(event.service);
      }
      return matching;
    };
    WaitFor([&] { return !dnssdResolved().empty(); }, 5);
    Sleep(1);
    size_t before = dnssdResolved().size();
    Check(before == 1, "resolved once before any change (" + std::to_string(before) + ")");
    Result updated;
    backend->Update("zc-dnssd", {{"v", "2"}}, updated.Resolve(), updated.Reject());
    updated.Wait(5);
    Check(updated.service && Txt(updated.service->txt, "v") == "2", "updateService resolves with the new TXT");
    auto withV2 = [&] {
      int count = 0;
      for (const auto &service : dnssdResolved()) count += Txt(service.txt, "v") == "2";
      return count;
    };
    WaitFor([&] { return withV2() > 0; }, 5);
    Check(withV2() == 1, "resolved again with the updated TXT record");
    Sleep(1);
    Check(dnssdResolved().size() == before + 1, "no duplicate resolved without a change");
    backend->Stop("L");
    Result updateUnknown;
    backend->Update("never", {}, updateUnknown.Resolve(), updateUnknown.Reject());
    updateUnknown.Wait(5);
    Check(updateUnknown.error && updateUnknown.error->code == "NOT_PUBLISHED", "updateService of an unknown service rejects NOT_PUBLISHED");

    // resolveService: one service by name, without scanning
    Result single;
    backend->ResolveService("zc-dnssd", "zcdns", "tcp", "local.", {}, single.Resolve(), single.Reject());
    single.Wait(5);
    Check(single.service && single.service->port == 45690 && Txt(single.service->txt, "v") == "2" && !single.service->addresses.empty(),
          "resolveService resolves port, TXT and addresses");
    Result missing;
    ResolveOptions oneSecond;
    oneSecond.timeoutSeconds = 1;
    auto missingStart = std::chrono::steady_clock::now();
    backend->ResolveService("zc-missing", "zcdns", "tcp", "local.", oneSecond, missing.Resolve(), missing.Reject());
    missing.Wait(5);
    double missingElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - missingStart).count();
    Check(missing.error && missing.error->code == "TIMEOUT" && missingElapsed < 1.5,
          "resolveService of a missing service rejects TIMEOUT after " + std::to_string(missingElapsed).substr(0, 3) + "s");

    // Subtypes: a subtype scan only finds the services registered with it
    Result withSubtype;
    PublishOptions subtypeOptions;
    subtypeOptions.subtypes = {"zcsub"};
    backend->Publish("zcdns", "tcp", "local.", "zc-sub", 45693, {}, subtypeOptions, withSubtype.Resolve(), withSubtype.Reject());
    withSubtype.Wait(5);
    Clear();
    ScanOptions subtypeScan;
    subtypeScan.subtype = "zcsub";
    backend->Scan("S", "zcdns", "tcp", "local.", subtypeScan);
    WaitFor([] { return !Named("resolved", "S").empty(); }, 5);
    Sleep(1);
    std::set<std::string> subNames;
    for (const auto &event : Named("found", "S")) subNames.insert(event.subject);
    Check(subNames == std::set<std::string>{"zc-sub"}, "subtype scan finds only zc-sub");
    backend->Stop("S");
    Result goneSub;
    backend->Unpublish("zc-sub", goneSub.Resolve(), goneSub.Reject());

    // Network interface: lo0 finds the local services, an unknown interface is an error
    Clear();
    ScanOptions loopback;
    loopback.networkInterface = "lo0";
    backend->Scan("I", "zcdns", "tcp", "local.", loopback);
    WaitFor([] { return !Named("resolved", "I").empty(); }, 5);
    auto loResolved = Named("resolved", "I");
    Check(!loResolved.empty() && loResolved[0].subject == "zc-dnssd", "scan on lo0 resolves zc-dnssd (" + (loResolved.empty() ? "" : Join(loResolved[0].service.addresses)) + ")");
    backend->Stop("I");
    Clear();
    ScanOptions nowhere;
    nowhere.networkInterface = "nope0";
    backend->Scan("X", "zcdns", "tcp", "local.", nowhere);
    WaitFor([] { return !Named("error", "X").empty(); }, 2);
    auto interfaceErrors = Named("error", "X");
    Check(interfaceErrors.size() == 1 && interfaceErrors[0].error.code == "UNKNOWN_INTERFACE", "unknown interface reports UNKNOWN_INTERFACE");

    // Teardown with a scan running and a service published
    backend->Scan("A", "zcdns", "tcp", "local.", scanOptions);
    backend->Shutdown();
  }
  Sleep(1);
  Check(weakBackend.expired(), "the backend is released after shutdown");
  printf("%s\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
  return failures ? 1 : 0;
}
