// Harness for windows/RNZeroconf/ZeroconfCore.cpp: publishes and browses real services through
// the Windows DNS-SD stack. Run with test/windows/run.cmd
#include "ZeroconfCore.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace rnzeroconf;

namespace {

struct Event {
  std::string name;
  std::string scanId;
  std::wstring subject;
  Service service;
  Error error;
};

std::mutex eventsMutex;
std::vector<Event> events;
int failures = 0;

void Add(Event event) {
  std::lock_guard<std::mutex> lock(eventsMutex);
  events.push_back(std::move(event));
}

std::vector<Event> Named(const std::string &name, const std::string &scanId = "") {
  std::lock_guard<std::mutex> lock(eventsMutex);
  std::vector<Event> result;
  for (auto &event : events) {
    if (event.name == name && (scanId.empty() || event.scanId == scanId)) result.push_back(event);
  }
  return result;
}

void Clear() {
  std::lock_guard<std::mutex> lock(eventsMutex);
  events.clear();
}

template <typename Predicate>
bool WaitFor(Predicate predicate, double seconds) {
  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(seconds * 1000));
  while (std::chrono::steady_clock::now() < end) {
    if (predicate()) return true;
    Sleep(50);
  }
  return predicate();
}

void Check(bool ok, const std::string &what) {
  printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  fflush(stdout);
  if (!ok) failures++;
}

std::string Text(const std::wstring &value) {
  return Narrow(value);
}

std::string TxtText(const TxtPairs &txt) {
  std::string text;
  for (auto &pair : txt) text += Text(pair.first) + "=" + Text(pair.second) + " ";
  return text;
}

std::string Join(const std::vector<std::wstring> &values) {
  std::string text;
  for (auto &value : values) text += (text.empty() ? "" : ",") + Text(value);
  return text;
}

// Promise-like result of an async call
struct Result {
  std::mutex mutex;
  bool settled = false;
  bool ok = false;
  Service service;
  Error error;
  ServiceCallback Resolve() {
    return [this](const Service &value) {
      std::lock_guard<std::mutex> lock(mutex);
      settled = true;
      ok = true;
      service = value;
    };
  }
  ErrorCallback Reject() {
    return [this](const Error &value) {
      std::lock_guard<std::mutex> lock(mutex);
      settled = true;
      error = value;
    };
  }
  bool Wait(double seconds) {
    return WaitFor([this] {
      std::lock_guard<std::mutex> lock(mutex);
      return settled;
    }, seconds);
  }
};

std::string ErrorText(const Error &error) {
  return Text(error.domain) + " " + (error.stringCode.empty() ? std::to_string(error.code) : Text(error.stringCode)) + " " + Text(error.message);
}

} // namespace

int main() {
  Events handlers;
  handlers.start = [](const std::string &scanId) { Add({"start", scanId}); };
  handlers.stop = [](const std::string &scanId) { Add({"stop", scanId}); };
  handlers.found = [](const std::string &scanId, const std::wstring &name) { Add({"found", scanId, name}); };
  handlers.remove = [](const std::string &scanId, const std::wstring &name) { Add({"remove", scanId, name}); };
  handlers.resolved = [](const std::string &scanId, const Service &service) {
    Event event{"resolved", scanId, service.name};
    event.service = service;
    Add(event);
  };
  handlers.published = [](const Service &service) {
    Event event{"published", "", service.name};
    event.service = service;
    Add(event);
  };
  handlers.unpublished = [](const Service &service) {
    Event event{"unpublished", "", service.name};
    event.service = service;
    Add(event);
  };
  handlers.error = [](const std::string &scanId, const Error &error) {
    Event event{"error", scanId, error.serviceName};
    event.error = error;
    Add(event);
    printf("     error event [%s] %s\n", scanId.c_str(), ErrorText(error).c_str());
  };

  {
    Zeroconf zeroconf(handlers);

    // Publish
    Result published;
    zeroconf.Publish(L"zcwin", L"tcp", L"local.", L"zc-win", 45700, {{L"b", L"2"}, {L"a", L"1"}}, L"", published.Resolve(), published.Reject());
    published.Wait(10);
    Check(published.ok && published.service.name == L"zc-win",
          "publish resolves with the name (" + Text(published.service.name) + " " + ErrorText(published.error) + ")");
    Check(Named("published").size() == 1, "published event emitted once");

    Result duplicate;
    zeroconf.Publish(L"zcwin", L"tcp", L"local.", L"zc-win", 45701, {}, L"", duplicate.Resolve(), duplicate.Reject());
    duplicate.Wait(10);
    Check(duplicate.ok && duplicate.service.name != L"zc-win", "duplicate name is renamed (" + Text(duplicate.service.name) + ")");

    // Scan: found then resolved with host, port, addresses and TXT
    Clear();
    zeroconf.Scan("A", L"zcwin", L"tcp", L"local.", L"", L"");
    Check(Named("start", "A").size() == 1, "start emitted");
    auto resolvedWin = [] {
      for (auto &event : Named("resolved", "A")) {
        if (event.subject == L"zc-win") return true;
      }
      return false;
    };
    WaitFor(resolvedWin, 20);
    Sleep(1000);
    std::set<std::wstring> foundNames;
    for (auto &event : Named("found", "A")) foundNames.insert(event.subject);
    Check(foundNames.count(L"zc-win") == 1, "found zc-win (" + std::to_string(Named("found", "A").size()) + " found events)");
    Check(Named("found", "A").size() == foundNames.size(), "found once per service");
    Event resolved;
    for (auto &event : Named("resolved", "A")) {
      if (event.subject == L"zc-win") resolved = event;
    }
    Check(resolved.service.port == 45700, "resolved port (" + std::to_string(resolved.service.port) + ")");
    Check(!resolved.service.host.empty(), "resolved host (" + Text(resolved.service.host) + ")");
    Check(!resolved.service.addresses.empty(), "resolved addresses (" + Join(resolved.service.addresses) + ")");
    Check(TxtText(resolved.service.txt).find("a=1") != std::string::npos && TxtText(resolved.service.txt).find("b=2") != std::string::npos,
          "resolved TXT (" + TxtText(resolved.service.txt) + ")");

    // resolveService: one service by name, without scanning
    Result single;
    zeroconf.ResolveService(L"zc-win", L"zcwin", L"tcp", L"local.", L"", 5, single.Resolve(), single.Reject());
    single.Wait(10);
    Check(single.ok && single.service.port == 45700, "resolveService resolves (" + std::to_string(single.service.port) + " " + ErrorText(single.error) + ")");
    Result missing;
    auto started = std::chrono::steady_clock::now();
    zeroconf.ResolveService(L"zc-missing", L"zcwin", L"tcp", L"local.", L"", 2, missing.Resolve(), missing.Reject());
    missing.Wait(10);
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    Check(!missing.ok && missing.settled && (missing.error.stringCode == L"TIMEOUT" || missing.error.domain == L"Windows"),
          "resolveService of a missing service rejects after " + std::to_string(elapsed) + "s (" + ErrorText(missing.error) + ")");

    // Update: published again with the new TXT record, under the same name
    Result updated;
    zeroconf.Update(L"zc-win", {{L"v", L"2"}}, updated.Resolve(), updated.Reject());
    updated.Wait(15);
    Check(updated.ok && updated.service.name == L"zc-win" && TxtText(updated.service.txt) == "v=2 ",
          "updateService publishes the new TXT (" + Text(updated.service.name) + " " + TxtText(updated.service.txt) + ErrorText(updated.error) + ")");
    Result afterUpdate;
    zeroconf.ResolveService(L"zc-win", L"zcwin", L"tcp", L"local.", L"", 5, afterUpdate.Resolve(), afterUpdate.Reject());
    afterUpdate.Wait(10);
    Check(TxtText(afterUpdate.service.txt).find("v=2") != std::string::npos, "resolves the updated TXT (" + TxtText(afterUpdate.service.txt) + ")");

    // Service types
    zeroconf.Scan("T", L"services._dns-sd", L"udp", L"local.", L"", L"");
    auto typeFound = [] {
      for (auto &event : Named("found", "T")) {
        if (event.subject == L"_zcwin._tcp") return true;
      }
      return false;
    };
    auto peerTypeFound = [] {
      for (auto &event : Named("found", "T")) {
        if (event.subject == L"_zcpeer._tcp") return true;
      }
      return false;
    };
    bool withPeer = GetEnvironmentVariableW(L"RNZEROCONF_PEER", nullptr, 0) > 0;
    WaitFor([&] { return typeFound() && (!withPeer || peerTypeFound()); }, 15);
    std::vector<std::wstring> types;
    for (auto &event : Named("found", "T")) types.push_back(event.subject);
    // The network doesn't answer the types of this machine's services, the library adds its own
    Check(typeFound(), "service types scan lists the type this app publishes (" + Join(types) + ")");
    if (withPeer) {
      Check(peerTypeFound(), "service types scan finds the peer's _zcpeer._tcp");
    }
    Check(Named("resolved", "T").empty(), "service types are not resolved");
    zeroconf.Stop("T");

    // A service published by another mDNS stack (test/app/peer.py)
    if (withPeer) {
      zeroconf.Scan("P", L"zcpeer", L"tcp", L"local.", L"", L"");
      auto peerResolved = [] { return !Named("resolved", "P").empty(); };
      WaitFor(peerResolved, 15);
      auto peer = Named("resolved", "P");
      Check(!peer.empty() && peer[0].subject == L"zc-peer" && peer[0].service.port == 45710 &&
                TxtText(peer[0].service.txt).find("from=python") != std::string::npos,
            "resolves a service published by another stack (" + (peer.empty() ? std::string("nothing") : Text(peer[0].service.host) + " " + std::to_string(peer[0].service.port) + " " + TxtText(peer[0].service.txt)) + ")");

      // Subtypes: zc-peer-sub is only registered under the _zcsub subtype
      zeroconf.Scan("S", L"zcpeer", L"tcp", L"local.", L"zcsub", L"");
      auto subResolved = [] { return !Named("resolved", "S").empty(); };
      WaitFor(subResolved, 15);
      Sleep(1000);
      std::vector<std::wstring> subFound, plainFound;
      for (auto &event : Named("found", "S")) subFound.push_back(event.subject);
      for (auto &event : Named("found", "P")) plainFound.push_back(event.subject);
      Check(subFound.size() == 1 && subFound[0] == L"zc-peer-sub", "subtype scan finds only zc-peer-sub (" + Join(subFound) + ")");
      Check(std::find(plainFound.begin(), plainFound.end(), L"zc-peer-sub") == plainFound.end(), "plain scan doesn't find the subtype-only service (" + Join(plainFound) + ")");
      zeroconf.Stop("S");

      // Live updates: the peer changes its TXT record, resolved is emitted again
      wchar_t temp[MAX_PATH] = {};
      GetTempPathW(MAX_PATH, temp);
      std::wstring trigger = std::wstring(temp) + L"rnzeroconf-peer-update";
      HANDLE file = CreateFileW(trigger.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
      auto updatedPeer = [] {
        for (auto &event : Named("resolved", "P")) {
          if (TxtText(event.service.txt).find("v=2") != std::string::npos) return true;
        }
        return false;
      };
      WaitFor(updatedPeer, 20);
      Check(updatedPeer(), "resolved again when the peer's TXT record changes (" + std::to_string(Named("resolved", "P").size()) + " resolved events)");
      zeroconf.Stop("P");

      // resolveTimeout: zc-ghost is announced but never answers, it times out, is retried once, then reported
      DWORD ghostStart = GetTickCount();
      zeroconf.Scan("G", L"zcghost", L"tcp", L"local.", L"", L"", 1);
      auto ghostTimedOut = [] {
        for (auto &event : Named("error", "G")) {
          if (event.error.stringCode == L"TIMEOUT") return true;
        }
        return false;
      };
      WaitFor(ghostTimedOut, 15);
      DWORD ghostElapsed = GetTickCount() - ghostStart;
      std::vector<std::wstring> ghostErrors;
      for (auto &event : Named("error", "G")) ghostErrors.push_back(event.error.stringCode + L" " + event.error.message);
      Check(ghostTimedOut(), "a service that never resolves is reported with TIMEOUT (" + Join(ghostErrors) + ")");
      Check(ghostElapsed >= 2000, "the timeout is retried once before the error (" + std::to_string(ghostElapsed) + " ms)");
      Check(Named("resolved", "G").empty(), "the service that never answers isn't resolved");
      zeroconf.Stop("G");
    }

    // Unpublish: the scan sees the service leave
    Result unpublished;
    zeroconf.Unpublish(L"zc-win", unpublished.Resolve(), unpublished.Reject());
    unpublished.Wait(10);
    Check(unpublished.ok && unpublished.service.name == L"zc-win", "unpublish resolves (" + ErrorText(unpublished.error) + ")");
    Check(Named("unpublished").size() == 1, "unpublished event emitted");
    auto removed = [] {
      for (auto &event : Named("remove", "A")) {
        if (event.subject == L"zc-win") return true;
      }
      return false;
    };
    WaitFor(removed, 10);
    // How Windows reports a service leaving isn't documented, this tells
    printf("info remove event for zc-win: %s\n", removed() ? "yes" : "no");

    Result unknown;
    zeroconf.Unpublish(L"never", unknown.Resolve(), unknown.Reject());
    unknown.Wait(5);
    Check(!unknown.ok && unknown.error.stringCode == L"NOT_PUBLISHED", "unpublish unknown rejects NOT_PUBLISHED");

    // Stop
    Clear();
    zeroconf.Stop("A");
    Check(Named("stop", "A").size() == 1, "stop emitted for A");

    // Network interface
    Clear();
    zeroconf.Scan("X", L"zcwin", L"tcp", L"local.", L"", L"nope0");
    auto interfaceErrors = Named("error", "X");
    Check(interfaceErrors.size() == 1 && interfaceErrors[0].error.stringCode == L"UNKNOWN_INTERFACE", "unknown interface reports UNKNOWN_INTERFACE");

    // Teardown with a service still published
    zeroconf.UnpublishAll();
    Sleep(1000);
  }

  printf("%s\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
  return failures ? 1 : 0;
}
