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
    expect(RN.lastCall()).toEqual(['registerService', 'http', 'tcp', 'local.', 'a', 80, [['b', '1'], ['a', 'true']], 'DNSSD'])
    expect(txt).toEqual({ b: 1, a: true })
  })

  test('takes TXT records as pairs to keep number-like keys in order', () => {
    new Zeroconf().publishService({ type: 'http', protocol: 'tcp', name: 'c', port: 82, txt: [['2', 'x'], ['1', 'y']] })
    expect(RN.lastCall()).toEqual(['registerService', 'http', 'tcp', 'local.', 'c', 82, [['2', 'x'], ['1', 'y']]])
  })

  test('still takes the deprecated positional form', () => {
    RN.Platform.OS = 'android'
    new Zeroconf().publishService('http', 'tcp', undefined, 'b', 81)
    expect(RN.lastCall()).toEqual(['registerService', 'http', 'tcp', 'local.', 'b', 81, [], 'NSD'])
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

  test('resolves unknown on Android without calling native code', async () => {
    RN.Platform.OS = 'android'
    await expect(new Zeroconf().checkLocalNetworkAccess()).resolves.toBe('unknown')
    expect(RN.callsNamed('checkLocalNetworkAccess')).toEqual([])
  })
})
