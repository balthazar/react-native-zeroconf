import Zeroconf, { ZeroconfError } from 'react-native-zeroconf'
const z = new Zeroconf()
z.on('error', (e: ZeroconfError) => { if (e.domain === 'DNSSD' && e.code === -65570) {}; const s: string | undefined = e.serviceName; return s })
