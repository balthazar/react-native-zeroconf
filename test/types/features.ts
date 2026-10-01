import Zeroconf, { ImplType, ResolveOptions, Service } from 'react-native-zeroconf'

const zeroconf = new Zeroconf()
zeroconf.scan({ type: 'ipp', subtype: 'printer', networkInterface: 'en0' })
zeroconf.scanServiceTypes({ networkInterface: 'wlan0' })
zeroconf.publishService({ type: 'ipp', protocol: 'tcp', name: 'p', port: 631, subtypes: ['printer'] })

async function features() {
  const updated = await zeroconf.updateService('p', { txt: { state: 'idle' } })
  const name: string = updated.name
  const options: ResolveOptions = { name: 'p', type: 'ipp', timeout: 3, implType: ImplType.DNSSD }
  const service: Service = await zeroconf.resolveService(options)
  const ipv4: string[] = service.ipv4
  const access: 'granted' | 'denied' | 'unknown' = await zeroconf.checkLocalNetworkAccess({ request: false })
  return [name, ipv4, access]
}

// @ts-expect-error resolveService needs a name
zeroconf.resolveService({ type: 'ipp' })
// @ts-expect-error subtypes is a list
zeroconf.publishService({ type: 'ipp', protocol: 'tcp', name: 'p', port: 631, subtypes: 'printer' })
