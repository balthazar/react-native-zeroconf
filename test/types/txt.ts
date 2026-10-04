import Zeroconf, { Service } from 'react-native-zeroconf'
const z = new Zeroconf()
z.on('resolved', (s: Service) => { const a: string = s.ipv4[0]; const b: string[] = s.ipv6; return a + b })
z.on('published', s => s.host)
z.publishService({ type: 'http', protocol: 'tcp', name: 'x', port: 80, txt: [['txtvers', 1], ['path', '/api']] })
z.publishService({ type: 'http', protocol: 'tcp', name: 'x', port: 80, txt: { a: 1, b: true } })
// @ts-expect-error pairs must be [key, value]
z.publishService({ type: 'http', protocol: 'tcp', name: 'x', port: 80, txt: [['only-key']] })
