const RN = require('react-native')
const { default: Zeroconf } = require('../src')

beforeEach(() => RN.reset())

describe('resolved services', () => {
  test('list IPv4 addresses first and expose ipv4 / ipv6', () => {
    const zeroconf = new Zeroconf()
    const resolved = jest.fn()
    zeroconf.on('resolved', resolved)
    RN.emit('RNZeroconfResolved', { name: 'a', addresses: ['fe80::1', '192.168.1.2', '::1', '10.0.0.1'] })

    const service = resolved.mock.calls[0][0]
    expect(service.addresses).toEqual(['192.168.1.2', '10.0.0.1', 'fe80::1', '::1'])
    expect(service.ipv4).toEqual(['192.168.1.2', '10.0.0.1'])
    expect(service.ipv6).toEqual(['fe80::1', '::1'])
  })

  test('without addresses get empty arrays', () => {
    const zeroconf = new Zeroconf()
    const resolved = jest.fn()
    zeroconf.on('resolved', resolved)
    RN.emit('RNZeroconfResolved', { name: 'b' })
    expect(resolved.mock.calls[0][0]).toMatchObject({ addresses: [], ipv4: [], ipv6: [] })
  })

  test('are kept in getServices, found ones only with their name', () => {
    const zeroconf = new Zeroconf()
    RN.emit('RNZeroconfFound', { name: 'a' })
    RN.emit('RNZeroconfFound', { name: 'b' })
    RN.emit('RNZeroconfResolved', { name: 'b', addresses: ['10.0.0.2'], port: 80 })
    RN.emit('RNZeroconfRemove', { name: 'a' })

    const services = zeroconf.getServices()
    expect(Object.keys(services)).toEqual(['b'])
    expect(services.b).toMatchObject({ name: 'b', port: 80, ipv4: ['10.0.0.2'] })
  })
})

describe('errors', () => {
  test('carry code, domain and serviceName', () => {
    const zeroconf = new Zeroconf()
    const error = jest.fn()
    zeroconf.on('error', error)
    RN.emit('RNZeroconfError', {
      message: 'Resolving service x failed: timed out',
      code: 'TIMEOUT',
      domain: 'RNZeroconf',
      serviceName: 'x',
    })

    const e = error.mock.calls[0][0]
    expect(e).toBeInstanceOf(Error)
    expect(e.message).toBe('Resolving service x failed: timed out')
    expect(e).toMatchObject({ code: 'TIMEOUT', domain: 'RNZeroconf', serviceName: 'x' })
  })

  test('have numeric codes for the platform domains', () => {
    const zeroconf = new Zeroconf()
    const error = jest.fn()
    zeroconf.on('error', error)
    RN.emit('RNZeroconfError', { message: 'not authorized', code: -65555, domain: 'DNSSD' })
    expect(error.mock.calls[0][0]).toMatchObject({ code: -65555, domain: 'DNSSD' })
  })

  test('are not thrown when nobody listens', () => {
    new Zeroconf()
    expect(() => RN.emit('RNZeroconfError', { message: 'x', code: 1, domain: 'DNSSD' })).not.toThrow()
  })
})

describe('subscribe', () => {
  test('returns a function removing the listener', () => {
    const zeroconf = new Zeroconf()
    const listener = jest.fn()
    const unsubscribe = zeroconf.subscribe('update', listener)
    zeroconf.emit('update')
    unsubscribe()
    zeroconf.emit('update')
    expect(listener).toHaveBeenCalledTimes(1)
  })

  test('leaves on() chainable', () => {
    const zeroconf = new Zeroconf()
    expect(zeroconf.on('start', () => {})).toBe(zeroconf)
  })
})

describe('multiple scans', () => {
  test('each instance receives only its own scan events', () => {
    const printers = new Zeroconf()
    const speakers = new Zeroconf()
    const listener = new Zeroconf()
    const got = { printers: [], speakers: [], listener: [] }
    for (const [key, zeroconf] of Object.entries({ printers, speakers, listener })) {
      zeroconf.on('found', name => got[key].push(`found ${name}`))
      zeroconf.on('resolved', service => got[key].push(`resolved ${service.name}`))
      zeroconf.on('error', e => got[key].push(`error ${e.code}`))
      zeroconf.on('published', service => got[key].push(`published ${service.name}`))
    }

    printers.scan({ type: 'ipp' })
    speakers.scan({ type: 'raop' })
    const [printersScan, speakersScan] = RN.callsNamed('scan').map(call => call[1])
    expect(printersScan).not.toBe(speakersScan)

    RN.emit('RNZeroconfFound', { name: 'Printer', scanId: printersScan })
    RN.emit('RNZeroconfResolved', { name: 'Speaker', addresses: [], scanId: speakersScan })
    RN.emit('RNZeroconfError', { message: 'x', code: 'TIMEOUT', domain: 'RNZeroconf', scanId: printersScan })
    RN.emit('RNZeroconfServiceRegistered', { name: 'Mine' })

    expect(got.printers).toEqual(['found Printer', 'error TIMEOUT', 'published Mine'])
    expect(got.speakers).toEqual(['resolved Speaker', 'published Mine'])
    // An instance that never scanned still receives everything
    expect(got.listener).toEqual(['found Printer', 'resolved Speaker', 'error TIMEOUT', 'published Mine'])
    expect(Object.keys(printers.getServices())).toEqual(['Printer'])
    expect(speakers.getServices().Speaker.scanId).toBeUndefined()
  })
})

test('removeDeviceListeners removes every native listener', () => {
  const zeroconf = new Zeroconf()
  expect(RN.listenerCount()).toBeGreaterThan(0)
  zeroconf.removeDeviceListeners()
  expect(RN.listenerCount()).toBe(0)
})
