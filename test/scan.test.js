const RN = require('react-native')
const { default: Zeroconf, ImplType } = require('../src')

beforeEach(() => RN.reset())

describe('scan on iOS', () => {
  test('defaults to _http._tcp on local. with a 5s resolve timeout', () => {
    new Zeroconf().scan()
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'http', 'tcp', 'local.', 5])
  })

  test('takes an options object', () => {
    new Zeroconf().scan({ type: 'printer', resolveTimeout: 15 })
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'printer', 'tcp', 'local.', 15])
  })

  test('still takes the deprecated positional form', () => {
    new Zeroconf().scan('ssh', 'udp')
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'ssh', 'udp', 'local.', 5])
  })

  test('stop passes the scan id', () => {
    const zeroconf = new Zeroconf()
    zeroconf.scan()
    const scanId = RN.lastCall()[1]
    zeroconf.stop()
    expect(RN.lastCall()).toEqual(['stop', scanId])
  })
})

describe('scan on Android', () => {
  beforeEach(() => {
    RN.Platform.OS = 'android'
  })

  test('defaults to NSD', () => {
    new Zeroconf().scan()
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'http', 'tcp', 'local.', 'NSD'])
  })

  test('passes implType, not resolveTimeout', () => {
    new Zeroconf().scan({ type: 'printer', implType: ImplType.DNSSD, resolveTimeout: 15 })
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'printer', 'tcp', 'local.', 'DNSSD'])
  })

  test('ignores undefined options instead of overriding the defaults', () => {
    new Zeroconf().scan({ type: 'ssh', domain: undefined })
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'ssh', 'tcp', 'local.', 'NSD'])
  })

  test('stop defaults to the implementation used by the last scan', () => {
    const zeroconf = new Zeroconf()
    zeroconf.scan({ implType: ImplType.DNSSD })
    zeroconf.stop()
    expect(RN.lastCallWithoutScanId()).toEqual(['stop', 'DNSSD'])
  })

  test('switching implementation stops the previous one first', () => {
    const zeroconf = new Zeroconf()
    zeroconf.scan({ implType: ImplType.DNSSD })
    zeroconf.scan({ type: 'ssh' })
    const [stop, scan] = RN.callsWithoutScanId().slice(-2)
    expect(stop).toEqual(['stop', 'DNSSD'])
    expect(scan).toEqual(['scan', 'ssh', 'tcp', 'local.', 'NSD'])
  })
})

test('the constructor throws a clear error without the native module', () => {
  jest.isolateModules(() => {
    const { NativeModules } = require('react-native')
    const native = NativeModules.RNZeroconf
    NativeModules.RNZeroconf = undefined
    try {
      const { default: Isolated } = require('../src')
      expect(() => new Isolated()).toThrow(/native module not found/)
    } finally {
      NativeModules.RNZeroconf = native
    }
  })
})
