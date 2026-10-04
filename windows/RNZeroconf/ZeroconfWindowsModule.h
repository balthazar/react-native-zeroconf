// The C++ module on Windows, registered with AddTurboModuleProvider in ReactPackageProvider.cpp:
// the windns.h backend (ZeroconfCore). Windows has no Local Network permission to grant.
#pragma once

#include "../../cpp/ZeroconfModule.h"

namespace facebook::react {

class ZeroconfWindowsModule : public ZeroconfModule {
 public:
  explicit ZeroconfWindowsModule(std::shared_ptr<CallInvoker> jsInvoker);
};

} // namespace facebook::react
