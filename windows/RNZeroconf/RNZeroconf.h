#pragma once

#include "pch.h"
#include "resource.h"

#include "NativeModules.h"
#include "ZeroconfCore.h"

#include <memory>
#include <string>

namespace winrt::RNZeroconf
{

// The RNZeroconf native module on Windows, with the same methods and events as on iOS.
// The DNS-SD work is done by ZeroconfCore (windns.h).
REACT_MODULE(RNZeroconf)
struct RNZeroconf
{
  REACT_INIT(Initialize)
  void Initialize(React::ReactContext const &reactContext) noexcept;

  // options: resolveTimeout, subtype, networkInterface
  REACT_METHOD(scan)
  void scan(std::string scanId, std::string type, std::string protocol, std::string domain, React::JSValue options) noexcept;

  // An empty scanId stops every scan
  REACT_METHOD(stop)
  void stop(React::JSValue scanId) noexcept;

  // txt: [key, value] pairs. options: subtypes, networkInterface
  REACT_METHOD(registerService)
  void registerService(
      std::string type,
      std::string protocol,
      std::string domain,
      std::string name,
      int port,
      React::JSValue txt,
      React::JSValue options,
      React::ReactPromise<React::JSValue> promise) noexcept;

  REACT_METHOD(unregisterService)
  void unregisterService(std::string name, React::ReactPromise<React::JSValue> promise) noexcept;

  REACT_METHOD(updateService)
  void updateService(std::string name, React::JSValue txt, React::ReactPromise<React::JSValue> promise) noexcept;

  // options: timeout, networkInterface
  REACT_METHOD(resolveService)
  void resolveService(
      std::string name,
      std::string type,
      std::string protocol,
      std::string domain,
      React::JSValue options,
      React::ReactPromise<React::JSValue> promise) noexcept;

  // Windows has no Local Network permission: resolves "granted"
  REACT_METHOD(checkLocalNetworkAccess)
  void checkLocalNetworkAccess(React::JSValue type, React::JSValue timeout, React::ReactPromise<React::JSValue> promise) noexcept;

private:
  void Emit(std::wstring_view eventName, React::JSValueObject body) noexcept;

  React::ReactContext m_context;
  std::unique_ptr<rnzeroconf::Zeroconf> m_zeroconf;
};

} // namespace winrt::RNZeroconf
