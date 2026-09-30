import Zeroconf, { useZeroconf, Service, ZeroconfError } from 'react-native-zeroconf'
const r = useZeroconf({ type: 'http', implType: 'DNSSD', enabled: true })
const s: Service[] = r.services; const e: ZeroconfError | null = r.error; const b: boolean = r.isScanning
r.stop(); r.restart()
const z = new Zeroconf(); const off: () => void = z.subscribe('resolved', (x: Service) => x.name); off()
z.on('found', () => {}).on('remove', () => {})
// @ts-expect-error unknown option
useZeroconf({ tipe: 'http' })
export { s, e, b }
