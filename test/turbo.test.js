// The C++ module (src/NativeZeroconf.ts) when it is registered, through src/bridge.ts
const RN = require('react-native')

const calls = []
const listeners = {}
const results = {}

const emitter = name => handler => {
  ;(listeners[name] = listeners[name] || []).push(handler)
  return {
    remove: () => {
      listeners[name] = listeners[name].filter(fn => fn !== handler)
    },
  }
}
const emit = (name, payload) => (listeners[name] || []).forEach(fn => fn(payload))

RN.turboModules.Zeroconf = new Proxy(
  {},
  {
    get: (_, method) => {
      if (method.startsWith('on')) {
        return emitter(method)
      }
      return (...args) => {
        calls.push([method, ...args])
        return Promise.resolve(results[method])
      }
    },
  },
)

const { default: Zeroconf, ImplType } = require('../src')

const nativeService = {
  name: 'printer',
  fullName: 'printer._ipp._tcp.local.',
  host: 'printer.local.',
  port: 631,
  addresses: ['fe80::1', '192.168.1.20'],
  txt: [
    { key: 'b', value: '2' },
    { key: 'a', value: '1' },
  ],
}

beforeEach(() => {
  calls.length = 0
  Object.keys(results).forEach(key => delete results[key])
  RN.Platform.OS = 'ios'
})

test('scan passes implType on every platform', () => {
  new Zeroconf().scan({ type: 'ipp', subtype: 'printer' })
  const [method, , ...args] = calls.at(-1)
  expect([method, ...args]).toEqual(['scan', 'ipp', 'tcp', 'local.', 'NSD', { resolveTimeout: 5, subtype: 'printer' }])
})

test('stop passes the scan id and implType', () => {
  const zeroconf = new Zeroconf()
  zeroconf.scan({ implType: ImplType.DNSSD })
  zeroconf.stop()
  const [method, scanId, implType] = calls.at(-1)
  expect([method, typeof scanId, implType]).toEqual(['stop', 'string', 'NSD'])
})

test('resolved events become services with a TXT object and address families', () => {
  const zeroconf = new Zeroconf()
  zeroconf.scan()
  const scanId = calls.at(-1)[1]
  const resolved = jest.fn()
  zeroconf.on('resolved', resolved)
  emit('onResolved', { scanId, service: nativeService })
  expect(resolved).toHaveBeenCalledWith({
    name: 'printer',
    fullName: 'printer._ipp._tcp.local.',
    host: 'printer.local.',
    port: 631,
    addresses: ['192.168.1.20', 'fe80::1'],
    ipv4: ['192.168.1.20'],
    ipv6: ['fe80::1'],
    txt: { b: '2', a: '1' },
  })
  expect(zeroconf.getServices().printer.port).toBe(631)
})

test('scan events of another scan are ignored', () => {
  const zeroconf = new Zeroconf()
  zeroconf.scan()
  const found = jest.fn()
  zeroconf.on('found', found)
  emit('onFound', { scanId: 'someone-else', name: 'printer' })
  emit('onFound', { scanId: calls.at(-1)[1], name: 'printer' })
  expect(found).toHaveBeenCalledTimes(1)
})

test('error codes are numbers for the platform domains and names for the library', () => {
  const zeroconf = new Zeroconf()
  zeroconf.scan()
  const scanId = calls.at(-1)[1]
  const errors = []
  zeroconf.on('error', error => errors.push(error))
  emit('onError', { scanId, error: { message: 'denied', code: '-65570', domain: 'DNSSD', serviceName: '' } })
  emit('onError', {
    scanId,
    error: { message: 'timed out', code: 'TIMEOUT', domain: 'RNZeroconf', serviceName: 'printer' },
  })
  expect(errors.map(error => [error.code, error.domain, error.serviceName])).toEqual([
    [-65570, 'DNSSD', undefined],
    ['TIMEOUT', 'RNZeroconf', 'printer'],
  ])
})

test('errors without a scan id reach every instance', () => {
  const zeroconf = new Zeroconf()
  zeroconf.scan()
  const errors = jest.fn()
  zeroconf.on('error', errors)
  emit('onError', {
    scanId: '',
    error: { message: 'too long', code: 'TXT_ENTRY_TOO_LONG', domain: 'RNZeroconf', serviceName: 'a' },
  })
  expect(errors).toHaveBeenCalledTimes(1)
})

test('publishService sends TXT entries in order and resolves with the service', async () => {
  results.registerService = { service: { ...nativeService, addresses: [] } }
  const published = await new Zeroconf().publishService({
    type: 'ipp',
    protocol: 'tcp',
    name: 'printer',
    port: 631,
    txt: [
      ['b', 2],
      ['a', 1],
    ],
  })
  expect(calls.at(-1)).toEqual([
    'registerService',
    'ipp',
    'tcp',
    'local.',
    'printer',
    631,
    [
      { key: 'b', value: '2' },
      { key: 'a', value: '1' },
    ],
    'NSD',
    {},
  ])
  expect(published.txt).toEqual({ b: '2', a: '1' })
})

test('an error result rejects with a ZeroconfError', async () => {
  results.unregisterService = {
    error: { message: 'Service x is not published', code: 'NOT_PUBLISHED', domain: 'RNZeroconf', serviceName: 'x' },
  }
  await expect(new Zeroconf().unpublishService('x')).rejects.toMatchObject({
    message: 'Service x is not published',
    code: 'NOT_PUBLISHED',
    domain: 'RNZeroconf',
    serviceName: 'x',
  })
})

test('resolveService resolves with address families', async () => {
  results.resolveService = { service: nativeService }
  const service = await new Zeroconf().resolveService({ name: 'printer', type: 'ipp', timeout: 2 })
  expect(calls.at(-1)).toEqual(['resolveService', 'printer', 'ipp', 'tcp', 'local.', 'NSD', { timeout: 2 }])
  expect(service.ipv4).toEqual(['192.168.1.20'])
})

test('checkLocalNetworkAccess resolves the status', async () => {
  results.checkLocalNetworkAccess = { status: 'granted' }
  await expect(new Zeroconf().checkLocalNetworkAccess({ type: 'http' })).resolves.toBe('granted')
  expect(calls.at(-1)).toEqual(['checkLocalNetworkAccess', '_http._tcp', 5])
})
