#include "NsdBackend.h"

#include <android/log.h>

#include <map>
#include <mutex>
#include <optional>
#include <utility>

using namespace facebook;

namespace rnzeroconf {

namespace {

constexpr const char *kHostClass = "com/balthazargronon/RCTZeroconf/NsdHost";
constexpr const char *kPromiseClass = "com/balthazargronon/RCTZeroconf/NsdPromise";
constexpr const char *kPayloadClass = "com/balthazargronon/RCTZeroconf/NsdPayload";

// What the Java side's handles point to: integers looked up here, so a late callback finds nothing
// instead of a freed object
struct Registry {
  std::mutex mutex;
  int64_t next = 0;
  std::map<int64_t, Events> hosts;
  std::map<int64_t, std::pair<ServiceCallback, ErrorCallback>> promises;
};

Registry &registry() {
  static Registry instance;
  return instance;
}

// Runs with the app's class loader, which finds the library's classes from any thread
template <typename F>
void WithEnv(F &&f) {
  jni::ThreadScope::WithClassLoader([&] { f(jni::Environment::current()); });
}

bool ClearException(JNIEnv *env, const char *what) {
  if (!env->ExceptionCheck()) {
    return false;
  }
  __android_log_print(ANDROID_LOG_ERROR, "RNZeroconf", "Java exception in %s", what);
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
}

jclass GlobalClass(const char *name) {
  return static_cast<jclass>(jni::Environment::current()->NewGlobalRef(jni::findClassLocal(name).get()));
}

jclass HostClass() {
  static jclass cls = GlobalClass(kHostClass);
  return cls;
}

jclass PromiseClass() {
  static jclass cls = GlobalClass(kPromiseClass);
  return cls;
}

jclass StringClass() {
  static jclass cls = GlobalClass("java/lang/String");
  return cls;
}

std::string ToString(jstring value) {
  return value ? jni::wrap_alias(value)->toStdString() : std::string();
}

std::vector<std::string> ToStrings(JNIEnv *env, jobjectArray array) {
  std::vector<std::string> strings;
  if (!array) {
    return strings;
  }
  jsize length = env->GetArrayLength(array);
  for (jsize i = 0; i < length; i++) {
    auto element = static_cast<jstring>(env->GetObjectArrayElement(array, i));
    strings.push_back(ToString(element));
    env->DeleteLocalRef(element);
  }
  return strings;
}

jobjectArray ToArray(JNIEnv *env, const std::vector<std::string> &strings) {
  jobjectArray array = env->NewObjectArray(static_cast<jsize>(strings.size()), StringClass(), nullptr);
  for (size_t i = 0; i < strings.size(); i++) {
    auto element = jni::make_jstring(strings[i]);
    env->SetObjectArrayElement(array, static_cast<jsize>(i), element.get());
  }
  return array;
}

// NsdPayload -> Service
Service ToService(JNIEnv *env, jobject payload) {
  Service service;
  if (!payload) {
    return service;
  }
  static jclass payloadClass = GlobalClass(kPayloadClass);
  static jmethodID getString = env->GetMethodID(payloadClass, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
  static jmethodID getInt = env->GetMethodID(payloadClass, "getInt", "(Ljava/lang/String;)I");
  static jmethodID getStrings = env->GetMethodID(payloadClass, "getStrings", "(Ljava/lang/String;)[Ljava/lang/String;");
  static jmethodID getMapKeys = env->GetMethodID(payloadClass, "getMapKeys", "(Ljava/lang/String;)[Ljava/lang/String;");
  static jmethodID getMapValues = env->GetMethodID(payloadClass, "getMapValues", "(Ljava/lang/String;)[Ljava/lang/String;");

  auto string = [&](const char *key) {
    auto name = jni::make_jstring(key);
    auto value = static_cast<jstring>(env->CallObjectMethod(payload, getString, name.get()));
    std::string result = ToString(value);
    env->DeleteLocalRef(value);
    return result;
  };
  auto strings = [&](jmethodID method, const char *key) {
    auto name = jni::make_jstring(key);
    auto value = static_cast<jobjectArray>(env->CallObjectMethod(payload, method, name.get()));
    auto result = ToStrings(env, value);
    env->DeleteLocalRef(value);
    return result;
  };

  service.name = string("name");
  service.fullName = string("fullName");
  service.host = string("host");
  {
    auto key = jni::make_jstring("port");
    service.port = static_cast<uint16_t>(env->CallIntMethod(payload, getInt, key.get()));
  }
  service.addresses = strings(getStrings, "addresses");
  auto keys = strings(getMapKeys, "txt");
  auto values = strings(getMapValues, "txt");
  for (size_t i = 0; i < keys.size() && i < values.size(); i++) {
    service.txt.emplace_back(keys[i], values[i]);
  }
  ClearException(env, "NsdPayload");
  return service;
}

std::optional<Events> HostEvents(jlong handle) {
  std::lock_guard<std::mutex> lock(registry().mutex);
  auto found = registry().hosts.find(handle);
  if (found == registry().hosts.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::optional<std::pair<ServiceCallback, ErrorCallback>> TakePromise(jlong id) {
  std::lock_guard<std::mutex> lock(registry().mutex);
  auto found = registry().promises.find(id);
  if (found == registry().promises.end()) {
    return std::nullopt;
  }
  auto callbacks = std::move(found->second);
  registry().promises.erase(found);
  return callbacks;
}

// NsdHost.nativeEvent: the event names of the Java side
void NativeEvent(JNIEnv *env, jclass, jlong handle, jstring eventName, jobject payload) {
  auto events = HostEvents(handle);
  if (!events) {
    return;
  }
  std::string event = ToString(eventName);
  Service service = ToService(env, payload);
  std::string scanId;
  if (payload) {
    static jclass payloadClass = GlobalClass(kPayloadClass);
    static jmethodID getString = env->GetMethodID(payloadClass, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
    auto key = jni::make_jstring("scanId");
    auto value = static_cast<jstring>(env->CallObjectMethod(payload, getString, key.get()));
    scanId = ToString(value);
    env->DeleteLocalRef(value);
  }
  if (event == "RNZeroconfStart" && events->start) {
    events->start(scanId);
  } else if (event == "RNZeroconfStop" && events->stop) {
    events->stop(scanId);
  } else if (event == "RNZeroconfFound" && events->found) {
    events->found(scanId, service.name);
  } else if (event == "RNZeroconfRemove" && events->remove) {
    events->remove(scanId, service.name);
  } else if (event == "RNZeroconfResolved" && events->resolved) {
    events->resolved(scanId, service);
  } else if (event == "RNZeroconfServiceRegistered" && events->published) {
    events->published(service);
  } else if (event == "RNZeroconfServiceUnregistered" && events->unpublished) {
    events->unpublished(service);
  }
}

void NativeError(JNIEnv *, jclass, jlong handle, jstring scanId, jstring domain, jstring code, jstring message, jstring serviceName) {
  auto events = HostEvents(handle);
  if (events && events->error) {
    events->error(ToString(scanId), Error{ToString(domain), ToString(code), ToString(message), ToString(serviceName)});
  }
}

void NativeResolve(JNIEnv *env, jclass, jlong id, jobject payload) {
  auto callbacks = TakePromise(id);
  if (callbacks && callbacks->first) {
    callbacks->first(ToService(env, payload));
  }
}

void NativeReject(JNIEnv *, jclass, jlong id, jstring domain, jstring code, jstring message, jstring serviceName) {
  auto callbacks = TakePromise(id);
  if (callbacks && callbacks->second) {
    callbacks->second(Error{ToString(domain), ToString(code), ToString(message), ToString(serviceName)});
  }
}

void RegisterNatives() {
  static std::once_flag once;
  std::call_once(once, [] {
    WithEnv([](JNIEnv *env) {
      JNINativeMethod hostMethods[] = {
          {const_cast<char *>("nativeEvent"), const_cast<char *>("(JLjava/lang/String;Lcom/balthazargronon/RCTZeroconf/NsdPayload;)V"),
           reinterpret_cast<void *>(NativeEvent)},
          {const_cast<char *>("nativeError"),
           const_cast<char *>("(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"),
           reinterpret_cast<void *>(NativeError)},
      };
      env->RegisterNatives(HostClass(), hostMethods, 2);
      ClearException(env, "registering NsdHost");
      JNINativeMethod promiseMethods[] = {
          {const_cast<char *>("nativeResolve"), const_cast<char *>("(JLcom/balthazargronon/RCTZeroconf/NsdPayload;)V"),
           reinterpret_cast<void *>(NativeResolve)},
          {const_cast<char *>("nativeReject"),
           const_cast<char *>("(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"),
           reinterpret_cast<void *>(NativeReject)},
      };
      env->RegisterNatives(PromiseClass(), promiseMethods, 2);
      ClearException(env, "registering NsdPromise");
    });
  });
}

// A new NsdPromise settling these callbacks
jobject NewPromise(JNIEnv *env, ServiceCallback resolve, ErrorCallback reject) {
  int64_t id;
  {
    std::lock_guard<std::mutex> lock(registry().mutex);
    id = ++registry().next;
    registry().promises[id] = {std::move(resolve), std::move(reject)};
  }
  static jmethodID constructor = env->GetMethodID(PromiseClass(), "<init>", "(J)V");
  return env->NewObject(PromiseClass(), constructor, static_cast<jlong>(id));
}

// Rejects a call whose promise never reached Java
void RejectLocally(jobject promise, JNIEnv *env, const std::string &name) {
  static jfieldID idField = env->GetFieldID(PromiseClass(), "id", "J");
  if (!promise) {
    return;
  }
  auto callbacks = TakePromise(env->GetLongField(promise, idField));
  if (callbacks && callbacks->second) {
    callbacks->second(LibraryError("EXCEPTION", "The NSD implementation failed", name));
  }
}

std::pair<std::vector<std::string>, std::vector<std::string>> Split(const TxtPairs &txt) {
  std::vector<std::string> keys;
  std::vector<std::string> values;
  for (const auto &[key, value] : txt) {
    keys.push_back(key);
    values.push_back(value);
  }
  return {keys, values};
}

} // namespace

std::shared_ptr<NsdBackend> NsdBackend::Create(Events events) {
  RegisterNatives();
  int64_t handle;
  {
    std::lock_guard<std::mutex> lock(registry().mutex);
    handle = ++registry().next;
    registry().hosts[handle] = std::move(events);
  }
  jni::global_ref<jobject> host;
  WithEnv([&](JNIEnv *env) {
    static jmethodID constructor = env->GetMethodID(HostClass(), "<init>", "(J)V");
    jobject local = env->NewObject(HostClass(), constructor, static_cast<jlong>(handle));
    if (!ClearException(env, "NsdHost()") && local) {
      host = jni::make_global(jni::wrap_alias(local));
      env->DeleteLocalRef(local);
    }
  });
  return std::shared_ptr<NsdBackend>(new NsdBackend(handle, std::move(host)));
}

NsdBackend::NsdBackend(int64_t handle, jni::global_ref<jobject> host) : handle_(handle), host_(std::move(host)) {}

NsdBackend::~NsdBackend() {
  std::lock_guard<std::mutex> lock(registry().mutex);
  registry().hosts.erase(handle_);
}

void NsdBackend::Scan(
    const std::string &scanId,
    const std::string &type,
    const std::string &protocol,
    const std::string &domain,
    const ScanOptions &options) {
  if (!host_) {
    return;
  }
  WithEnv([&](JNIEnv *env) {
    static jmethodID scan = env->GetMethodID(
        HostClass(), "scan", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;D)V");
    env->CallVoidMethod(host_.get(), scan, jni::make_jstring(scanId).get(), jni::make_jstring(type).get(), jni::make_jstring(protocol).get(),
                        jni::make_jstring(domain).get(), jni::make_jstring(options.subtype).get(), jni::make_jstring(options.networkInterface).get(),
                        options.resolveTimeoutSeconds);
    ClearException(env, "NsdHost.scan");
  });
}

void NsdBackend::Stop(const std::string &scanId) {
  if (!host_) {
    return;
  }
  WithEnv([&](JNIEnv *env) {
    static jmethodID stop = env->GetMethodID(HostClass(), "stop", "(Ljava/lang/String;)V");
    env->CallVoidMethod(host_.get(), stop, jni::make_jstring(scanId).get());
    ClearException(env, "NsdHost.stop");
  });
}

void NsdBackend::Publish(
    const std::string &type,
    const std::string &protocol,
    const std::string &domain,
    const std::string &name,
    uint16_t port,
    const TxtPairs &txt,
    const PublishOptions &options,
    ServiceCallback resolve,
    ErrorCallback reject) {
  if (!host_) {
    reject(LibraryError("EXCEPTION", "The NSD implementation is not available", name));
    return;
  }
  WithEnv([&](JNIEnv *env) {
    static jmethodID registerService = env->GetMethodID(
        HostClass(), "registerService",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;Lcom/balthazargronon/RCTZeroconf/NsdPromise;)V");
    auto [keys, values] = Split(txt);
    jobjectArray keyArray = ToArray(env, keys);
    jobjectArray valueArray = ToArray(env, values);
    jobjectArray subtypeArray = ToArray(env, options.subtypes);
    jobject promise = NewPromise(env, std::move(resolve), std::move(reject));
    env->CallVoidMethod(host_.get(), registerService, jni::make_jstring(type).get(), jni::make_jstring(protocol).get(), jni::make_jstring(domain).get(),
                        jni::make_jstring(name).get(), static_cast<jint>(port), keyArray, valueArray, subtypeArray,
                        jni::make_jstring(options.networkInterface).get(), promise);
    if (ClearException(env, "NsdHost.registerService")) {
      RejectLocally(promise, env, name);
    }
    env->DeleteLocalRef(keyArray);
    env->DeleteLocalRef(valueArray);
    env->DeleteLocalRef(subtypeArray);
    env->DeleteLocalRef(promise);
  });
}

void NsdBackend::Unpublish(const std::string &name, ServiceCallback resolve, ErrorCallback reject) {
  if (!host_) {
    reject(LibraryError("EXCEPTION", "The NSD implementation is not available", name));
    return;
  }
  WithEnv([&](JNIEnv *env) {
    static jmethodID unregisterService =
        env->GetMethodID(HostClass(), "unregisterService", "(Ljava/lang/String;Lcom/balthazargronon/RCTZeroconf/NsdPromise;)V");
    jobject promise = NewPromise(env, std::move(resolve), std::move(reject));
    env->CallVoidMethod(host_.get(), unregisterService, jni::make_jstring(name).get(), promise);
    if (ClearException(env, "NsdHost.unregisterService")) {
      RejectLocally(promise, env, name);
    }
    env->DeleteLocalRef(promise);
  });
}

void NsdBackend::Update(const std::string &name, const TxtPairs &txt, ServiceCallback resolve, ErrorCallback reject) {
  if (!host_) {
    reject(LibraryError("EXCEPTION", "The NSD implementation is not available", name));
    return;
  }
  WithEnv([&](JNIEnv *env) {
    static jmethodID updateService = env->GetMethodID(
        HostClass(), "updateService", "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;Lcom/balthazargronon/RCTZeroconf/NsdPromise;)V");
    auto [keys, values] = Split(txt);
    jobjectArray keyArray = ToArray(env, keys);
    jobjectArray valueArray = ToArray(env, values);
    jobject promise = NewPromise(env, std::move(resolve), std::move(reject));
    env->CallVoidMethod(host_.get(), updateService, jni::make_jstring(name).get(), keyArray, valueArray, promise);
    if (ClearException(env, "NsdHost.updateService")) {
      RejectLocally(promise, env, name);
    }
    env->DeleteLocalRef(keyArray);
    env->DeleteLocalRef(valueArray);
    env->DeleteLocalRef(promise);
  });
}

void NsdBackend::ResolveService(
    const std::string &name,
    const std::string &type,
    const std::string &protocol,
    const std::string &domain,
    const ResolveOptions &options,
    ServiceCallback resolve,
    ErrorCallback reject) {
  if (!host_) {
    reject(LibraryError("EXCEPTION", "The NSD implementation is not available", name));
    return;
  }
  WithEnv([&](JNIEnv *env) {
    static jmethodID resolveService = env->GetMethodID(
        HostClass(), "resolveService",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;DLcom/balthazargronon/RCTZeroconf/NsdPromise;)V");
    jobject promise = NewPromise(env, std::move(resolve), std::move(reject));
    env->CallVoidMethod(host_.get(), resolveService, jni::make_jstring(name).get(), jni::make_jstring(type).get(), jni::make_jstring(protocol).get(),
                        jni::make_jstring(domain).get(), jni::make_jstring(options.networkInterface).get(), options.timeoutSeconds, promise);
    if (ClearException(env, "NsdHost.resolveService")) {
      RejectLocally(promise, env, name);
    }
    env->DeleteLocalRef(promise);
  });
}

void NsdBackend::Shutdown() {
  if (!host_) {
    return;
  }
  WithEnv([&](JNIEnv *env) {
    static jmethodID shutdown = env->GetMethodID(HostClass(), "shutdown", "()V");
    env->CallVoidMethod(host_.get(), shutdown);
    ClearException(env, "NsdHost.shutdown");
  });
  std::lock_guard<std::mutex> lock(registry().mutex);
  registry().hosts.erase(handle_);
}

std::string NsdBackend::CheckLocalNetworkAccess() {
  std::string status = "unknown";
  WithEnv([&](JNIEnv *env) {
    static jmethodID check = env->GetStaticMethodID(HostClass(), "checkLocalNetworkAccess", "()Ljava/lang/String;");
    auto result = static_cast<jstring>(env->CallStaticObjectMethod(HostClass(), check));
    if (!ClearException(env, "NsdHost.checkLocalNetworkAccess")) {
      status = ToString(result);
    }
    env->DeleteLocalRef(result);
  });
  return status;
}

} // namespace rnzeroconf
