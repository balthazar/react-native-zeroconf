// Stand-in for react-native: records native calls and lets tests emit native events

const calls = []
const handlers = {}
const nativeResults = {}

// Scan and stop calls start with the instance's scan id, `callsWithoutScanId` drops it for readability
const withoutScanId = call =>
  (call[0] === 'scan' || call[0] === 'stop') &&
  typeof call[1] === 'string' &&
  call[1].startsWith('zeroconf-')
    ? [call[0], ...call.slice(2)]
    : call

const RNZeroconf = new Proxy(
  {},
  {
    get: (_, method) =>
      (...args) => {
        calls.push([method, ...args])
        const result = nativeResults[method]
        return typeof result === 'function' ? result(...args) : Promise.resolve(result)
      },
  },
)

module.exports = {
  Platform: { OS: 'ios' },
  NativeModules: { RNZeroconf },
  DeviceEventEmitter: {
    addListener(name, fn) {
      ;(handlers[name] = handlers[name] || []).push(fn)
      return {
        remove() {
          handlers[name] = handlers[name].filter(f => f !== fn)
        },
      }
    },
  },

  // Test helpers
  calls,
  lastCall: () => calls[calls.length - 1],
  callsNamed: name => calls.filter(c => c[0] === name),
  callsWithoutScanId: () => calls.map(withoutScanId),
  lastCallWithoutScanId: () => withoutScanId(calls[calls.length - 1]),
  emit: (name, payload) => (handlers[name] || []).forEach(fn => fn(payload)),
  listenerCount: () => Object.values(handlers).reduce((n, list) => n + list.length, 0),
  // What a native method returns: a value, or a function returning a promise
  setNativeResult: (method, result) => {
    nativeResults[method] = result
  },
  reset() {
    calls.length = 0
    Object.keys(handlers).forEach(k => delete handlers[k])
    Object.keys(nativeResults).forEach(k => delete nativeResults[k])
    module.exports.Platform.OS = 'ios'
  },
}
