#include "ZeroconfAndroidModule.h"

#include "../dnssd/DnssdBackend.h"
#include "../embedded/EmbeddedExecutor.h"

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
  // NSD is still the Java module, JavaScript sends only DNSSD calls here
  platform.backendKey = [](const std::string &) { return std::string("DNSSD"); };
  platform.createBackend = [](const std::string &, rnzeroconf::Events events) -> std::shared_ptr<rnzeroconf::Backend> {
    auto executor = rnzeroconf::EmbeddedExecutor::Shared();
    if (executor->StartError() != 0) {
      __android_log_print(ANDROID_LOG_ERROR, "RNZeroconf", "The embedded mDNSResponder failed to start (%d)", executor->StartError());
    }
    return rnzeroconf::DnssdBackend::Create(executor, std::move(events));
  };
  platform.setActive = SetMulticastLock;
  return platform;
}

} // namespace

ZeroconfAndroidModule::ZeroconfAndroidModule(std::shared_ptr<CallInvoker> jsInvoker)
    : ZeroconfModule(std::move(jsInvoker), AndroidPlatform()) {}

} // namespace facebook::react
