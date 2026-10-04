// Android while NSD is still the previous module: DNSSD goes to the C++ module, NSD to the previous one
const RN = require('react-native')

const cxxCalls = []
const cxxListeners = {}
RN.turboModules.Zeroconf = new Proxy(
  {},
  {
    get: (_, method) => {
      if (method.startsWith('on')) {
        return handler => {
          ;(cxxListeners[method] = cxxListeners[method] || []).push(handler)
          return { remove: () => {} }
        }
      }
      return (...args) => {
        cxxCalls.push([method, ...args])
        return Promise.resolve({ service: { name: 'x', fullName: '', host: '', port: 1, addresses: [], txt: [] } })
      }
    },
  },
)
RN.Platform.OS = 'android'

const { default: Zeroconf, ImplType } = require('../src')

beforeEach(() => {
  cxxCalls.length = 0
  RN.Platform.OS = 'android'
})

test('DNSSD scans go to the C++ module', () => {
  new Zeroconf().scan({ implType: ImplType.DNSSD })
  expect(cxxCalls.at(-1).slice(2)).toEqual(['http', 'tcp', 'local.', 'DNSSD', { resolveTimeout: 5 }])
})

test('NSD scans go to the previous module', () => {
  RN.reset()
  RN.Platform.OS = 'android'
  new Zeroconf().scan({ implType: ImplType.NSD })
  expect(cxxCalls).toEqual([])
  expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'http', 'tcp', 'local.', 'NSD', { resolveTimeout: 5 }])
})

test('events from both modules reach the instance', () => {
  const zeroconf = new Zeroconf()
  const found = jest.fn()
  zeroconf.on('found', found)
  cxxListeners.onFound.forEach(fn => fn({ scanId: '', name: 'from-cxx' }))
  RN.emit('RNZeroconfFound', { name: 'from-previous' })
  expect(found.mock.calls.map(([name]) => name)).toEqual(['from-cxx', 'from-previous'])
})

test('publishing with DNSSD goes to the C++ module', async () => {
  await new Zeroconf().publishService({ type: 'http', protocol: 'tcp', name: 'x', port: 1, implType: ImplType.DNSSD })
  expect(cxxCalls.at(-1)[0]).toBe('registerService')
})
