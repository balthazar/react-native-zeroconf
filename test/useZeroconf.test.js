global.IS_REACT_ACT_ENVIRONMENT = true

const React = require('react')
const TestRenderer = require('react-test-renderer')
const RN = require('react-native')
const { useZeroconf } = require('../src')

const { act } = TestRenderer

beforeEach(() => RN.reset())

// Renders the hook and exposes its latest result
function renderHook(initialOptions) {
  const result = {}
  function Probe({ options }) {
    Object.assign(result, useZeroconf(options))
    return null
  }
  let root
  act(() => {
    root = TestRenderer.create(React.createElement(Probe, { options: initialOptions }))
  })
  return {
    result,
    rerender: options => act(() => root.update(React.createElement(Probe, { options }))),
    unmount: () => act(() => root.unmount()),
  }
}

const scans = () => RN.callsNamed('scan')

test('scans on mount with the options', () => {
  renderHook({ type: 'http', resolveTimeout: 7 })
  expect(RN.lastCallWithoutScanId()).toEqual(['scan', 'http', 'tcp', 'local.', 7])
})

test('lists resolved services only, and follows removals', () => {
  const { result } = renderHook({ type: 'http' })
  act(() => RN.emit('RNZeroconfStart', {}))
  expect(result.isScanning).toBe(true)

  act(() => RN.emit('RNZeroconfFound', { name: 'a' }))
  expect(result.services).toEqual([])

  act(() => RN.emit('RNZeroconfResolved', { name: 'a', host: 'a.local.', port: 80, addresses: ['fe80::1', '10.0.0.2'], txt: {} }))
  expect(result.services).toHaveLength(1)
  expect(result.services[0].ipv4).toEqual(['10.0.0.2'])

  act(() => RN.emit('RNZeroconfRemove', { name: 'a' }))
  expect(result.services).toEqual([])
})

test('exposes the last error', () => {
  const { result } = renderHook({ type: 'http' })
  act(() => RN.emit('RNZeroconfError', { message: 'boom', code: -65570, domain: 'DNSSD' }))
  expect(result.error).toMatchObject({ code: -65570, domain: 'DNSSD' })
})

test('scans again when the options change, not on every render', () => {
  const { result, rerender } = renderHook({ type: 'http' })
  act(() => RN.emit('RNZeroconfError', { message: 'boom', code: 1, domain: 'DNSSD' }))

  rerender({ type: 'ipp' })
  expect(scans()).toHaveLength(2)
  expect(RN.lastCallWithoutScanId().slice(0, 2)).toEqual(['scan', 'ipp'])
  expect(result.error).toBeNull()

  rerender({ type: 'ipp' })
  expect(scans()).toHaveLength(2)
})

test('restart scans again and stop stops', () => {
  const { result } = renderHook({ type: 'http' })
  act(() => result.restart())
  expect(scans()).toHaveLength(2)

  const stops = RN.callsNamed('stop').length
  act(() => result.stop())
  expect(RN.callsNamed('stop').length).toBeGreaterThan(stops)
  act(() => RN.emit('RNZeroconfStop', {}))
  expect(result.isScanning).toBe(false)
})

test('does not scan while disabled', () => {
  const { rerender } = renderHook({ type: 'http', enabled: false })
  expect(scans()).toHaveLength(0)
  rerender({ type: 'http', enabled: true })
  expect(scans()).toHaveLength(1)
})

test('stops and removes every native listener on unmount', () => {
  const { unmount } = renderHook({ type: 'http' })
  const stops = RN.callsNamed('stop').length
  unmount()
  expect(RN.callsNamed('stop').length).toBeGreaterThan(stops)
  expect(RN.listenerCount()).toBe(0)
})

test('several hooks run separate scans', () => {
  renderHook({ type: 'ipp' })
  renderHook({ type: 'raop' })
  const [first, second] = scans()
  expect(first[1]).not.toBe(second[1])
  expect([first[2], second[2]]).toEqual(['ipp', 'raop'])
})
