import Zeroconf, { ScanOptions, PublishOptions } from 'react-native-zeroconf'
const z = new Zeroconf()
z.scan()
z.scan({ type: 'printer', implType: 'DNSSD', resolveTimeout: 15 })
z.scan('http', 'tcp', 'local.', 'NSD')
z.publishService({ type: 'http', protocol: 'tcp', name: 'x', port: 80, txt: [['a', '1']] })
z.publishService('http', 'tcp', undefined, 'x', 80)
const o: ScanOptions = {}; const p: PublishOptions = { type: 'http', protocol: 'tcp', name: 'x', port: 1 }
// @ts-expect-error name and port are required
z.publishService({ type: 'http', protocol: 'tcp' })
// @ts-expect-error unknown option
z.scan({ typo: 'http' })
