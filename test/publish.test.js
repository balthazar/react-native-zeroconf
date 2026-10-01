const RN = require('react-native')
const { default: Zeroconf, ImplType } = require('../src')

beforeEach(() => RN.reset())

const nativeRejection = (code, userInfo) => {
  const error = new Error('native')
  error.code = code
  error.userInfo = userInfo
  return Promise.reject(error)
}

describe('publishService', () => {
  test('sends TXT records as ordered string pairs, without mutating the input', () => {
    RN.Platform.OS = 'android'
    const txt = { b: 1, a: true }
    new Zeroconf().publishService({ type: 'http', protocol: 'tcp', name: 'a', port: 80, txt, implType: ImplType.DNSSD })
    expect(RN.lastCall()).toEqual(['registerService', 'http', 'tcp', 'local.', 'a', 80, [['b', '1'], ['a', 'true']], 'DNSSD', {}])
    expect(txt).toEqual({ b: 1, a: true })
  })

  test('takes TXT records as pairs to keep number-like keys in order', () => {
    new Zeroconf().publishService({ type: 'http', protocol: 'tcp', name: 'c', port: 82, txt: [['2', 'x'], ['1', 'y']] })
    expect(RN.lastCall()).toEqual(['registerService', 'http', 'tcp', 'local.', 'c', 82, [['2', 'x'], ['1', 'y']], {}])
  })

  test('still takes the deprecated positional form', () => {
    RN.Platform.OS = 'android'
    new Zeroconf().publishService('http', 'tcp', undefined, 'b', 81)
    expect(RN.lastCall()).toEqual(['registerService', 'http', 'tcp', 'local.', 'b', 81, [], 'NSD', {}])
  })

  test('resolves with the published service, possibly renamed', async () => {
    RN.setNativeResult('registerService', { name: 'Web (2)', port: 80 })
    const service = await new Zeroconf().publishService({ type: 'http', protocol: 'tcp', name: 'Web', port: 80 })
    expect(service.name).toBe('Web (2)')
  })

  test('rejects with a structured error', async () => {
    RN.setNativeResult('registerService', () =>
      nativeRejection('-65540', {
        message: 'Publishing service x failed: bad parameter',
        code: -65540,
        domain: 'DNSSD',
        serviceName: 'x',
      }),
    )
    await expect(new Zeroconf().publishService({ type: 'bad', protocol: 'nope', name: 'x', port: 1 })).rejects.toMatchObject({
      message: 'Publishing service x failed: bad parameter',
      code: -65540,
      domain: 'DNSSD',
      serviceName: 'x',
    })
  })

  test('does not cause unhandled rejections when not awaited', async () => {
    const unhandled = jest.fn()
    process.on('unhandledRejection', unhandled)
    RN.setNativeResult('registerService', () => nativeRejection('X', { message: 'm', code: 'X', domain: 'RNZeroconf' }))
    new Zeroconf().publishService({ type: 'a', protocol: 'tcp', name: 'n', port: 1 })
    await new Promise(resolve => setTimeout(resolve, 20))
    process.off('unhandledRejection', unhandled)
    expect(unhandled).not.toHaveBeenCalled()
  })
})

describe('publish options', () => {
  test('pass subtypes and the network interface', () => {
    new Zeroconf().publishService({ type: 'ipp', protocol: 'tcp', name: 'p', port: 631, subtypes: ['printer'], networkInterface: 'en0' })
    expect(RN.lastCall()).toEqual(['registerService', 'ipp', 'tcp', 'local.', 'p', 631, [], { subtypes: ['printer'], networkInterface: 'en0' }])
  })
})

describe('updateService', () => {
  test('sends the new TXT record', async () => {
    RN.setNativeResult('updateService', { name: 'a', txt: { v: '2' } })
    await expect(new Zeroconf().updateService('a', { txt: { v: 2 } })).resolves.toMatchObject({ txt: { v: '2' } })
    expect(RN.lastCall()).toEqual(['updateService', 'a', [['v', '2']]])
  })

  test('uses the implementation the service was published with on Android', () => {
    RN.Platform.OS = 'android'
    const zeroconf = new Zeroconf()
    zeroconf.publishService({ type: 'http', protocol: 'tcp', name: 'a', port: 80, implType: ImplType.DNSSD })
    zeroconf.updateService('a', { txt: { v: '2' } })
    expect(RN.lastCall()).toEqual(['updateService', 'a', [['v', '2']], 'DNSSD'])
  })

  test('rejects NOT_PUBLISHED for unknown services', async () => {
    RN.setNativeResult('updateService', () =>
      nativeRejection('NOT_PUBLISHED', { message: 'Service x is not published', code: 'NOT_PUBLISHED', domain: 'RNZeroconf', serviceName: 'x' }),
    )
    await expect(new Zeroconf().updateService('x', { txt: {} })).rejects.toMatchObject({ code: 'NOT_PUBLISHED' })
  })
})

describe('resolveService', () => {
  test('resolves with address families', async () => {
    RN.setNativeResult('resolveService', { name: 'p', port: 631, addresses: ['fe80::1', '10.0.0.3'], txt: {} })
    const service = await new Zeroconf().resolveService({ name: 'p', type: 'ipp' })
    expect(service).toMatchObject({ addresses: ['10.0.0.3', 'fe80::1'], ipv4: ['10.0.0.3'], ipv6: ['fe80::1'] })
    expect(RN.lastCall()).toEqual(['resolveService', 'p', 'ipp', 'tcp', 'local.', { timeout: 5 }])
  })

  test('passes the implementation and options on Android', () => {
    RN.Platform.OS = 'android'
    new Zeroconf().resolveService({ name: 'p', type: 'ipp', implType: ImplType.DNSSD, timeout: 10, networkInterface: 'wlan0' })
    expect(RN.lastCall()).toEqual(['resolveService', 'p', 'ipp', 'tcp', 'local.', 'DNSSD', { timeout: 10, networkInterface: 'wlan0' }])
  })

  test('rejects with TIMEOUT', async () => {
    RN.setNativeResult('resolveService', () =>
      nativeRejection('TIMEOUT', { message: 'Resolving service p failed: timed out', code: 'TIMEOUT', domain: 'RNZeroconf', serviceName: 'p' }),
    )
    await expect(new Zeroconf().resolveService({ name: 'p' })).rejects.toMatchObject({ code: 'TIMEOUT', serviceName: 'p' })
  })
})

describe('unpublishService', () => {
  test('defaults to the implementation used to publish', () => {
    RN.Platform.OS = 'android'
    const zeroconf = new Zeroconf()
    zeroconf.publishService({ type: 'http', protocol: 'tcp', name: 'a', port: 80, implType: ImplType.DNSSD })
    zeroconf.unpublishService('a')
    expect(RN.lastCall()).toEqual(['unregisterService', 'a', 'DNSSD'])
  })

  test('rejects NOT_PUBLISHED for unknown services', async () => {
    RN.setNativeResult('unregisterService', () =>
      nativeRejection('NOT_PUBLISHED', { message: 'Service y is not published', code: 'NOT_PUBLISHED', domain: 'RNZeroconf', serviceName: 'y' }),
    )
    await expect(new Zeroconf().unpublishService('y')).rejects.toMatchObject({ code: 'NOT_PUBLISHED', domain: 'RNZeroconf' })
  })

  test('passes through rejections without structured info', async () => {
    RN.setNativeResult('unregisterService', () => nativeRejection('E_OTHER', undefined))
    await expect(new Zeroconf().unpublishService('z')).rejects.toMatchObject({ code: 'E_OTHER' })
  })
})

describe('checkLocalNetworkAccess', () => {
  test('uses the first NSBonjourServices entry by default', async () => {
    RN.setNativeResult('checkLocalNetworkAccess', 'granted')
    await expect(new Zeroconf().checkLocalNetworkAccess()).resolves.toBe('granted')
    expect(RN.lastCall()).toEqual(['checkLocalNetworkAccess', null, 5])
  })

  test('builds the service type from type and protocol', async () => {
    const zeroconf = new Zeroconf()
    await zeroconf.checkLocalNetworkAccess({ type: 'http', timeout: 10 })
    expect(RN.lastCall()).toEqual(['checkLocalNetworkAccess', '_http._tcp', 10])
    await zeroconf.checkLocalNetworkAccess({ type: 'dns', protocol: 'udp' })
    expect(RN.lastCall()).toEqual(['checkLocalNetworkAccess', '_dns._udp', 5])
  })

  test('resolves granted on Android when there is nothing to grant', async () => {
    RN.Platform.OS = 'android'
    RN.setNativeResult('checkLocalNetworkAccess', 'granted')
    await expect(new Zeroconf().checkLocalNetworkAccess()).resolves.toBe('granted')
    expect(RN.callsNamed('PermissionsAndroid.request')).toEqual([])
  })

  test('requests ACCESS_LOCAL_NETWORK on Android when it is missing', async () => {
    RN.Platform.OS = 'android'
    RN.setNativeResult('checkLocalNetworkAccess', 'denied')
    RN.setNativeResult('PermissionsAndroid.request', 'granted')
    await expect(new Zeroconf().checkLocalNetworkAccess()).resolves.toBe('granted')
    expect(RN.callsNamed('PermissionsAndroid.request')).toEqual([
      ['PermissionsAndroid.request', 'android.permission.ACCESS_LOCAL_NETWORK'],
    ])
  })

  test('only checks on Android with request: false', async () => {
    RN.Platform.OS = 'android'
    RN.setNativeResult('checkLocalNetworkAccess', 'denied')
    await expect(new Zeroconf().checkLocalNetworkAccess({ request: false })).resolves.toBe('denied')
    expect(RN.callsNamed('PermissionsAndroid.request')).toEqual([])
  })
})
