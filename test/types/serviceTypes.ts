import Zeroconf, { ImplType, ServiceType, useServiceTypes } from 'react-native-zeroconf'

const zeroconf = new Zeroconf()
zeroconf.scanServiceTypes()
zeroconf.scanServiceTypes({ implType: ImplType.DNSSD })
zeroconf.on('typeFound', ({ type, protocol }) => zeroconf.scan({ type, protocol }))
const types: ServiceType[] = zeroconf.getServiceTypes()

const { serviceTypes, isScanning } = useServiceTypes({ enabled: types.length === 0 })
const first: string | undefined = serviceTypes[0]?.type
const scanning: boolean = isScanning

// @ts-expect-error service types only have tcp or udp protocols
const bad: ServiceType = { type: 'http', protocol: 'sctp' }
