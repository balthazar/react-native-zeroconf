const RN = require('react-native')
const { default: Zeroconf, ImplType } = require('../src')

beforeEach(() => RN.reset())

describe('scan on iOS', () => {
  test('defaults to _http._tcp on local. with a 5s resolve timeout', () => {
    new Zeroconf().scan()
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'http', 'tcp', 'local.', 'NSD', { resolveTimeout: 5 }])
  })

  test('takes an options object', () => {
    new Zeroconf().scan({ type: 'printer', resolveTimeout: 15 })
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'printer', 'tcp', 'local.', 'NSD', { resolveTimeout: 15 }])
  })

  test('rejects the positional form removed in 1.0', () => {
    expect(() => new Zeroconf().scan('ssh', 'udp')).toThrow(/scan\(\) takes an options object since 1.0/)
    expect(RN.callsNamed('scan')).toEqual([])
  })

  test('stop passes the scan id', () => {
    const zeroconf = new Zeroconf()
    zeroconf.scan()
    const scanId = RN.lastCall()[1]
    zeroconf.stop()
    expect(RN.lastCall()).toEqual(['stop', scanId, 'NSD'])
  })
})

describe('scan on Android', () => {
  beforeEach(() => {
    RN.Platform.OS = 'android'
  })

  test('defaults to NSD', () => {
    new Zeroconf().scan()
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'http', 'tcp', 'local.', 'NSD', { resolveTimeout: 5 }])
  })

  test('passes implType and resolveTimeout', () => {
    new Zeroconf().scan({ type: 'printer', implType: ImplType.DNSSD, resolveTimeout: 15 })
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'printer', 'tcp', 'local.', 'DNSSD', { resolveTimeout: 15 }])
  })

  test('ignores undefined options instead of overriding the defaults', () => {
    new Zeroconf().scan({ type: 'ssh', domain: undefined })
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'ssh', 'tcp', 'local.', 'NSD', { resolveTimeout: 5 }])
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
    expect(scan).toEqual(['scan', 'ssh', 'tcp', 'local.', 'NSD', { resolveTimeout: 5 }])
  })
})

describe('scan options', () => {
  test('pass the subtype and network interface on iOS', () => {
    new Zeroconf().scan({ type: 'ipp', subtype: 'printer', networkInterface: 'en0' })
    expect(RN.lastCallWithoutScanId()).toEqual([
      'scan',
      'ipp',
      'tcp',
      'local.',
      'NSD',
      { resolveTimeout: 5, subtype: 'printer', networkInterface: 'en0' },
    ])
  })

  test('pass the subtype and network interface on Android', () => {
    RN.Platform.OS = 'android'
    new Zeroconf().scan({ type: 'ipp', subtype: '_printer', networkInterface: 'wlan0' })
    expect(RN.lastCallWithoutScanId()).toEqual([
      'scan',
      'ipp',
      'tcp',
      'local.',
      'NSD',
      { resolveTimeout: 5, subtype: '_printer', networkInterface: 'wlan0' },
    ])
  })
})

test('the constructor throws a clear error without the native module', () => {
  jest.isolateModules(() => {
    const { turboModules } = require('react-native')
    const native = turboModules.Zeroconf
    delete turboModules.Zeroconf
    try {
      const { default: Isolated } = require('../src')
      expect(() => new Isolated()).toThrow(/native module not found/)
    } finally {
      turboModules.Zeroconf = native
    }
  })
})
