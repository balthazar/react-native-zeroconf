// The C++ module on Android, constructed by React Native's autolinking (react-native.config.js):
// Android's NsdManager for NSD (through the Java side), the mDNSResponder embedded in the library for
// DNSSD, and the multicast lock
#pragma once

#include "../ZeroconfModule.h"

namespace facebook::react {

class ZeroconfAndroidModule : public ZeroconfModule {
 public:
  explicit ZeroconfAndroidModule(std::shared_ptr<CallInvoker> jsInvoker);
};

} // namespace facebook::react
