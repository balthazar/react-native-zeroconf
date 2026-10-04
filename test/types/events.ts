import Zeroconf, { ImplType, Service } from 'react-native-zeroconf'
const z = new Zeroconf()
z.on('resolved', (s: Service) => s.addresses.map(a => a.length))
z.on('remove', (name: string) => name.toUpperCase())
z.scan({ type: 'http', protocol: 'tcp', domain: 'local.', implType: ImplType.DNSSD })
z.publishService({ type: 'http', protocol: 'tcp', name: 'x', port: 80, txt: { a: 1 } })
z.stop()
const n: number = z.listenerCount('found')
// @ts-expect-error wrong payload type
z.on('found', (s: Service) => s)
// @ts-expect-error bad impl type
z.scan({ type: 'http', implType: 'FOO' })
