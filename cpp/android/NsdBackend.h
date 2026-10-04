// The backend for Android's NsdManager (NSD), which is a Java API: it drives the NSD code of the library's
// Java side (NsdHost) through JNI, events and results come back through native methods.
#pragma once

#include "../Backend.h"

#include <fbjni/fbjni.h>

#include <memory>
#include <string>

namespace rnzeroconf {

class NsdBackend : public Backend {
 public:
  static std::shared_ptr<NsdBackend> Create(Events events);
  ~NsdBackend() override;

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

  // "granted", "denied" or "unknown": Android 17's ACCESS_LOCAL_NETWORK permission
  static std::string CheckLocalNetworkAccess();

 private:
  NsdBackend(int64_t handle, facebook::jni::global_ref<jobject> host);

  int64_t handle_;
  facebook::jni::global_ref<jobject> host_;
};

} // namespace rnzeroconf
