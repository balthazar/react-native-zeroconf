#include "ZeroconfWindowsModule.h"

#include "WindowsBackend.h"

namespace facebook::react {

namespace {

ZeroconfPlatform WindowsPlatform() {
  ZeroconfPlatform platform;
  // One mDNS stack on Windows, implType is Android's
  platform.backendKey = [](const std::string &) { return std::string("windns"); };
  platform.createBackend = [](const std::string &, rnzeroconf::Events events) { return rnzeroconf::CreateWindowsBackend(std::move(events)); };
  return platform;
}

} // namespace

ZeroconfWindowsModule::ZeroconfWindowsModule(std::shared_ptr<CallInvoker> jsInvoker)
    : ZeroconfModule(std::move(jsInvoker), WindowsPlatform()) {}

} // namespace facebook::react
