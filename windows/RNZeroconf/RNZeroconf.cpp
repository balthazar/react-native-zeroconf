#include "pch.h"

#include "RNZeroconf.h"

using namespace rnzeroconf;

namespace winrt::RNZeroconf
{

namespace {

std::wstring Wide(std::string const &value) {
  return Widen(value);
}

std::wstring StringOption(React::JSValue const &options, char const *key) {
  if (options.Type() != React::JSValueType::Object) {
    return L"";
  }
  auto const &value = options.AsObject();
  auto it = value.find(key);
  return it != value.end() && it->second.Type() == React::JSValueType::String ? Wide(it->second.AsString()) : L"";
}

double NumberOption(React::JSValue const &options, char const *key) {
  if (options.Type() != React::JSValueType::Object) {
    return 0;
  }
  auto const &value = options.AsObject();
  auto it = value.find(key);
  return it != value.end() && (it->second.Type() == React::JSValueType::Double || it->second.Type() == React::JSValueType::Int64)
      ? it->second.AsDouble()
      : 0;
}

// [[key, value], ...] from JavaScript
TxtPairs ReadTxt(React::JSValue const &txt) {
  TxtPairs pairs;
  if (txt.Type() != React::JSValueType::Array) {
    return pairs;
  }
  for (auto const &pair : txt.AsArray()) {
    if (pair.Type() == React::JSValueType::Array && pair.AsArray().size() == 2) {
      pairs.emplace_back(Wide(pair.AsArray()[0].AsString()), Wide(pair.AsArray()[1].AsString()));
    }
  }
  return pairs;
}

React::JSValueObject ServiceObject(Service const &service) {
  React::JSValueArray addresses;
  for (auto const &address : service.addresses) {
    addresses.push_back(Narrow(address));
  }
  React::JSValueObject txt;
  for (auto const &pair : service.txt) {
    txt[Narrow(pair.first)] = Narrow(pair.second);
  }
  React::JSValueObject object;
  object["name"] = Narrow(service.name);
  object["fullName"] = Narrow(service.fullName);
  object["host"] = Narrow(service.host);
  object["port"] = static_cast<int64_t>(service.port);
  object["addresses"] = std::move(addresses);
  object["txt"] = std::move(txt);
  return object;
}

React::JSValueObject ErrorObject(Error const &error) {
  React::JSValueObject object;
  object["message"] = Narrow(error.message);
  if (error.stringCode.empty()) {
    object["code"] = error.code;
  } else {
    object["code"] = Narrow(error.stringCode);
  }
  object["domain"] = Narrow(error.domain);
  if (!error.serviceName.empty()) {
    object["serviceName"] = Narrow(error.serviceName);
  }
  return object;
}

// Rejects with the same { message, code, domain, serviceName } in userInfo as iOS and Android
void Reject(React::ReactPromise<React::JSValue> const &promise, Error const &error) {
  React::ReactError reactError;
  reactError.Code = error.stringCode.empty() ? std::to_string(error.code) : Narrow(error.stringCode);
  reactError.Message = Narrow(error.message);
  reactError.UserInfo = ErrorObject(error);
  promise.Reject(std::move(reactError));
}

} // namespace

void RNZeroconf::Initialize(React::ReactContext const &reactContext) noexcept {
  m_context = reactContext;

  Events events;
  events.start = [this](std::string const &scanId) {
    React::JSValueObject body;
    body["scanId"] = scanId;
    Emit(L"RNZeroconfStart", std::move(body));
  };
  events.stop = [this](std::string const &scanId) {
    React::JSValueObject body;
    body["scanId"] = scanId;
    Emit(L"RNZeroconfStop", std::move(body));
  };
  events.found = [this](std::string const &scanId, std::wstring const &name) {
    React::JSValueObject body;
    body["name"] = Narrow(name);
    body["scanId"] = scanId;
    Emit(L"RNZeroconfFound", std::move(body));
  };
  events.remove = [this](std::string const &scanId, std::wstring const &name) {
    React::JSValueObject body;
    body["name"] = Narrow(name);
    body["scanId"] = scanId;
    Emit(L"RNZeroconfRemove", std::move(body));
  };
  events.resolved = [this](std::string const &scanId, Service const &service) {
    React::JSValueObject body = ServiceObject(service);
    body["scanId"] = scanId;
    Emit(L"RNZeroconfResolved", std::move(body));
  };
  events.published = [this](Service const &service) { Emit(L"RNZeroconfServiceRegistered", ServiceObject(service)); };
  events.unpublished = [this](Service const &service) { Emit(L"RNZeroconfServiceUnregistered", ServiceObject(service)); };
  events.error = [this](std::string const &scanId, Error const &error) {
    React::JSValueObject body = ErrorObject(error);
    if (!scanId.empty()) {
      body["scanId"] = scanId;
    }
    Emit(L"RNZeroconfError", std::move(body));
  };
  m_zeroconf = std::make_unique<Zeroconf>(std::move(events));
}

void RNZeroconf::Emit(std::wstring_view eventName, React::JSValueObject body) noexcept {
  m_context.EmitJSEvent(L"RCTDeviceEventEmitter", eventName, std::move(body));
}

void RNZeroconf::scan(std::string scanId, std::string type, std::string protocol, std::string domain, React::JSValue options) noexcept {
  m_zeroconf->Scan(scanId, Wide(type), Wide(protocol), Wide(domain), StringOption(options, "subtype"), StringOption(options, "networkInterface"),
                    NumberOption(options, "resolveTimeout"));
}

void RNZeroconf::stop(React::JSValue scanId) noexcept {
  if (scanId.Type() == React::JSValueType::String && !scanId.AsString().empty()) {
    m_zeroconf->Stop(scanId.AsString());
  } else {
    m_zeroconf->StopAll();
  }
}

void RNZeroconf::registerService(
    std::string type,
    std::string protocol,
    std::string domain,
    std::string name,
    int port,
    React::JSValue txt,
    React::JSValue options,
    React::ReactPromise<React::JSValue> promise) noexcept {
  // windns.h can't register subtypes
  if (options.Type() == React::JSValueType::Object) {
    auto const &object = options.AsObject();
    auto subtypes = object.find("subtypes");
    if (subtypes != object.end() && subtypes->second.Type() == React::JSValueType::Array && !subtypes->second.AsArray().empty()) {
      Error error;
      error.domain = L"RNZeroconf";
      error.stringCode = L"UNSUPPORTED";
      error.message = L"Publishing subtypes is not supported on Windows";
      error.serviceName = Wide(name);
      Reject(promise, error);
      return;
    }
  }
  m_zeroconf->Publish(
      Wide(type),
      Wide(protocol),
      Wide(domain),
      Wide(name),
      static_cast<uint16_t>(port),
      ReadTxt(txt),
      StringOption(options, "networkInterface"),
      [promise](Service const &service) mutable { promise.Resolve(React::JSValue(ServiceObject(service))); },
      [promise](Error const &error) mutable { Reject(promise, error); });
}

void RNZeroconf::unregisterService(std::string name, React::ReactPromise<React::JSValue> promise) noexcept {
  m_zeroconf->Unpublish(
      Wide(name),
      [promise](Service const &service) mutable { promise.Resolve(React::JSValue(ServiceObject(service))); },
      [promise](Error const &error) mutable { Reject(promise, error); });
}

void RNZeroconf::updateService(std::string name, React::JSValue txt, React::ReactPromise<React::JSValue> promise) noexcept {
  m_zeroconf->Update(
      Wide(name),
      ReadTxt(txt),
      [promise](Service const &service) mutable { promise.Resolve(React::JSValue(ServiceObject(service))); },
      [promise](Error const &error) mutable { Reject(promise, error); });
}

void RNZeroconf::resolveService(
    std::string name,
    std::string type,
    std::string protocol,
    std::string domain,
    React::JSValue options,
    React::ReactPromise<React::JSValue> promise) noexcept {
  m_zeroconf->ResolveService(
      Wide(name),
      Wide(type),
      Wide(protocol),
      Wide(domain),
      StringOption(options, "networkInterface"),
      NumberOption(options, "timeout"),
      [promise](Service const &service) mutable { promise.Resolve(React::JSValue(ServiceObject(service))); },
      [promise](Error const &error) mutable { Reject(promise, error); });
}

void RNZeroconf::checkLocalNetworkAccess(React::JSValue, React::JSValue, React::ReactPromise<React::JSValue> promise) noexcept {
  // Nothing to grant on Windows, as on Android before Android 17
  promise.Resolve(React::JSValue("granted"));
}

} // namespace winrt::RNZeroconf
