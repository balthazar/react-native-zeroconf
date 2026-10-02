const React = require('react')
const TestRenderer = require('react-test-renderer')
const RN = require('react-native')
const { default: Zeroconf, ImplType, useServiceTypes } = require('../src')

global.IS_REACT_ACT_ENVIRONMENT = true
const { act } = TestRenderer

beforeEach(() => RN.reset())

describe('scanServiceTypes', () => {
  test('browses _services._dns-sd._udp', () => {
    new Zeroconf().scanServiceTypes()
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'services._dns-sd', 'udp', 'local.', { resolveTimeout: 5 }])
  })

  test('defaults to NSD on Android', () => {
    RN.Platform.OS = 'android'
    const zeroconf = new Zeroconf()
    zeroconf.scanServiceTypes()
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'services._dns-sd', 'udp', 'local.', 'NSD', { resolveTimeout: 5 }])
    zeroconf.scanServiceTypes({ implType: ImplType.DNSSD })
    expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'services._dns-sd', 'udp', 'local.', 'DNSSD', { resolveTimeout: 5 }])
  })

  test('reports types, not services', () => {
    const zeroconf = new Zeroconf()
    const events = []
    zeroconf.on('typeFound', t => events.push(['typeFound', t]))
    zeroconf.on('typeRemove', t => events.push(['typeRemove', t]))
    zeroconf.on('found', name => events.push(['found', name]))
    zeroconf.scanServiceTypes()
    const scanId = RN.lastCall()[1]

    RN.emit('RNZeroconfFound', { name: '_http._tcp', scanId })
    RN.emit('RNZeroconfFound', { name: '_airplay._tcp', scanId })
    RN.emit('RNZeroconfFound', { name: '_http._tcp', scanId })
    RN.emit('RNZeroconfFound', { name: 'not a type', scanId })
    RN.emit('RNZeroconfRemove', { name: '_http._tcp', scanId })

    expect(events).toEqual([
      ['typeFound', { type: 'http', protocol: 'tcp' }],
      ['typeFound', { type: 'airplay', protocol: 'tcp' }],
      ['typeRemove', { type: 'http', protocol: 'tcp' }],
    ])
    expect(zeroconf.getServiceTypes()).toEqual([{ type: 'airplay', protocol: 'tcp' }])
    expect(zeroconf.getServices()).toEqual({})
  })

  test('a regular scan afterwards reports services again', () => {
    const zeroconf = new Zeroconf()
    const found = jest.fn()
    zeroconf.on('found', found)
    zeroconf.scanServiceTypes()
    RN.emit('RNZeroconfFound', { name: '_http._tcp', scanId: RN.lastCall()[1] })

    zeroconf.scan({ type: 'http' })
    expect(zeroconf.getServiceTypes()).toEqual([])
    RN.emit('RNZeroconfFound', { name: 'Printer', scanId: RN.lastCall()[1] })
    expect(found).toHaveBeenCalledWith('Printer')
  })
})

test('useServiceTypes lists the types while mounted', () => {
  const result = {}
  function Probe() {
    Object.assign(result, useServiceTypes())
    return null
  }
  let root
  act(() => {
    root = TestRenderer.create(React.createElement(Probe))
  })
  const scanId = RN.lastCall()[1]
  expect(RN.lastCallWithoutScanId().slice(0, 3)).toEqual(['scan', 'services._dns-sd', 'udp'])

  act(() => RN.emit('RNZeroconfStart', { scanId }))
  act(() => RN.emit('RNZeroconfFound', { name: '_ipp._tcp', scanId }))
  expect(result.isScanning).toBe(true)
  expect(result.serviceTypes).toEqual([{ type: 'ipp', protocol: 'tcp' }])

  act(() => root.unmount())
  expect(RN.listenerCount()).toBe(0)
})
