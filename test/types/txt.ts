import Zeroconf, { Service } from 'react-native-zeroconf'
const z = new Zeroconf()
z.on('resolved', (s: Service) => { const a: string = s.ipv4[0]; const b: string[] = s.ipv6; return a + b })
z.on('published', s => s.host)
z.publishService('http', 'tcp', 'local.', 'x', 80, [['txtvers', 1], ['path', '/api']])
z.publishService('http', 'tcp', 'local.', 'x', 80, { a: 1, b: true })
// @ts-expect-error pairs must be [key, value]
z.publishService('http', 'tcp', 'local.', 'x', 80, [['only-key']])
