#include "ZeroconfAndroidModule.h"

#include "../dnssd/DnssdBackend.h"
#include "../embedded/EmbeddedExecutor.h"
#include "NsdBackend.h"

#include <android/log.h>
#include <fbjni/fbjni.h>

namespace facebook::react {

namespace {

// ZeroconfNativeSupport.setMulticastLock, from any thread
void SetMulticastLock(bool held) {
  jni::ThreadScope::WithClassLoader([held] {
    static const auto support = jni::findClassStatic("com/balthazargronon/RCTZeroconf/ZeroconfNativeSupport");
    static const auto setMulticastLock = support->getStaticMethod<void(jboolean)>("setMulticastLock");
    setMulticastLock(support, held ? JNI_TRUE : JNI_FALSE);
  });
}

ZeroconfPlatform AndroidPlatform() {
  ZeroconfPlatform platform;
  // NSD (the default) is Android's NsdManager, DNSSD the mDNSResponder embedded in the library
  platform.backendKey = [](const std::string &implType) { return std::string(implType == "DNSSD" ? "DNSSD" : "NSD"); };
  platform.createBackend = [](const std::string &key, rnzeroconf::Events events) -> std::shared_ptr<rnzeroconf::Backend> {
    if (key == "NSD") {
      return rnzeroconf::NsdBackend::Create(std::move(events));
    }
    auto executor = rnzeroconf::EmbeddedExecutor::Shared();
    if (executor->StartError() != 0) {
      __android_log_print(ANDROID_LOG_ERROR, "RNZeroconf", "The embedded mDNSResponder failed to start (%d)", executor->StartError());
    }
    return rnzeroconf::DnssdBackend::Create(executor, std::move(events));
  };
  platform.setActive = SetMulticastLock;
  // Android 17's ACCESS_LOCAL_NETWORK, JavaScript requests it when it is denied
  platform.checkLocalNetworkAccess = [](const std::string &, double, std::function<void(const std::string &)> resolve,
                                        std::function<void(const rnzeroconf::Error &)>) { resolve(rnzeroconf::NsdBackend::CheckLocalNetworkAccess()); };
  return platform;
}

} // namespace

ZeroconfAndroidModule::ZeroconfAndroidModule(std::shared_ptr<CallInvoker> jsInvoker)
    : ZeroconfModule(std::move(jsInvoker), AndroidPlatform()) {}

} // namespace facebook::react
