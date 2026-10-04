import Zeroconf, { PublishedService } from 'react-native-zeroconf'
const z = new Zeroconf()
async function f() {
  const s: PublishedService = await z.publishService({ type: 'http', protocol: 'tcp', name: 'x', port: 1 })
  const u: PublishedService | null = await z.unpublishService(s.name)
  return [s, u]
}
f()
