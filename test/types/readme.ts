import Zeroconf, { ImplType, Service } from 'react-native-zeroconf'
const zeroconf = new Zeroconf()
zeroconf.on('resolved', (service: Service) => {
  console.log(service.name, service.addresses, service.txt)
})
zeroconf.on('remove', name => console.log(`${name} left`))
zeroconf.scan({ type: 'http', implType: ImplType.DNSSD })
