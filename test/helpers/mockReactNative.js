// Stand-in for react-native: a fake of the C++ module (src/NativeZeroconf.ts) that records calls,
// and helpers to emit its events and set its results in the shapes the JavaScript API works with

const calls = []
const listeners = {}
const nativeResults = {}

// Scan and stop calls start with the instance's scan id, `callsWithoutScanId` drops it for readability
const withoutScanId = call =>
  (call[0] === 'scan' || call[0] === 'stop') &&
  typeof call[1] === 'string' &&
  call[1].startsWith('zeroconf-')
    ? [call[0], ...call.slice(2)]
    : call

// A service as the JavaScript API sees it -> as the C++ module sends it
const toNativeService = service => ({
  name: service.name,
  fullName: service.fullName ?? '',
  host: service.host ?? '',
  port: service.port ?? 0,
  addresses: service.addresses ?? [],
  txt: Object.entries(service.txt ?? {}).map(([key, value]) => ({ key, value: String(value) })),
})

const toNativeError = error => ({
  message: error.message ?? '',
  code: String(error.code),
  domain: error.domain ?? 'RNZeroconf',
  serviceName: error.serviceName ?? '',
})

// Results are { service } or { error }: a test sets a service, or a function returning a rejected promise
const settle = (method, args) =>
  Promise.resolve(typeof nativeResults[method] === 'function' ? nativeResults[method](...args) : nativeResults[method]).then(
    value =>
      method === 'checkLocalNetworkAccess'
        ? { status: value ?? 'unknown' }
        : value
          ? { service: toNativeService(value) }
          : {},
    rejection => ({ error: toNativeError(rejection?.userInfo ?? rejection ?? {}) }),
  )

const emitter = name => handler => {
  ;(listeners[name] = listeners[name] || []).push(handler)
  return {
    remove() {
      listeners[name] = listeners[name].filter(fn => fn !== handler)
    },
  }
}

const Zeroconf = new Proxy(
  {},
  {
    get: (_, method) => {
      if (typeof method !== 'string') {
        return undefined
      }
      if (method.startsWith('on')) {
        return emitter(method)
      }
      return (...args) => {
        calls.push([method, ...args])
        return method === 'scan' || method === 'stop' ? undefined : settle(method, args)
      }
    },
  },
)

// TurboModules by name, the C++ module by default
const turboModules = { Zeroconf }

// The event names of the tests -> the C++ module's events and payloads
const EVENTS = {
  RNZeroconfStart: ['onStart', ({ scanId }) => ({ scanId: scanId ?? '' })],
  RNZeroconfStop: ['onStop', ({ scanId }) => ({ scanId: scanId ?? '' })],
  RNZeroconfFound: ['onFound', ({ scanId, name }) => ({ scanId: scanId ?? '', name })],
  RNZeroconfRemove: ['onRemove', ({ scanId, name }) => ({ scanId: scanId ?? '', name })],
  RNZeroconfResolved: ['onResolved', ({ scanId, ...service }) => ({ scanId: scanId ?? '', service: toNativeService(service) })],
  RNZeroconfError: ['onError', ({ scanId, ...error }) => ({ scanId: scanId ?? '', error: toNativeError(error) })],
  RNZeroconfServiceRegistered: ['onPublished', service => toNativeService(service)],
  RNZeroconfServiceUnregistered: ['onUnpublished', service => toNativeService(service)],
}

module.exports = {
  Platform: { OS: 'ios' },
  TurboModuleRegistry: {
    get: name => turboModules[name] || null,
  },
  turboModules,
  PermissionsAndroid: {
    RESULTS: { GRANTED: 'granted', DENIED: 'denied', NEVER_ASK_AGAIN: 'never_ask_again' },
    request(permission) {
      calls.push(['PermissionsAndroid.request', permission])
      return Promise.resolve(nativeResults['PermissionsAndroid.request'] || 'denied')
    },
  },

  // Test helpers
  calls,
  lastCall: () => calls[calls.length - 1],
  callsNamed: name => calls.filter(c => c[0] === name),
  callsWithoutScanId: () => calls.map(withoutScanId),
  lastCallWithoutScanId: () => withoutScanId(calls[calls.length - 1]),
  emit: (name, payload) => {
    const [event, convert] = EVENTS[name]
    ;(listeners[event] || []).forEach(fn => fn(convert(payload || {})))
  },
  listenerCount: () => Object.values(listeners).reduce((n, list) => n + list.length, 0),
  // What a native method settles with: a value, or a function returning a promise
  setNativeResult: (method, result) => {
    nativeResults[method] = result
  },
  reset() {
    calls.length = 0
    Object.keys(listeners).forEach(k => delete listeners[k])
    Object.keys(nativeResults).forEach(k => delete nativeResults[k])
    module.exports.Platform.OS = 'ios'
  },
}
