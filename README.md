<p align="center">
  <img src=".github/banner.png" alt="react-native-zeroconf: Bonjour / mDNS discovery and publishing for React Native, on iOS, Android, macOS, tvOS and Expo" width="100%">
</p>

Find the services other devices advertise on the local network (printers, speakers, cameras, your own servers) and advertise your own, on iOS, Android, macOS and tvOS.

**[Read the documentation](https://zeroconf.balthazar.dev)**

## Installation

```bash
yarn add react-native-zeroconf
cd ios && pod install
```

On iOS, declare the service types you use in `NSBonjourServices`, see [Permissions and Setup](https://zeroconf.balthazar.dev/Permissions-and-Setup). Expo works with development builds, see [Installation](https://zeroconf.balthazar.dev/Installation).

## Usage

```javascript
import { useZeroconf } from 'react-native-zeroconf'

function Printers() {
  const { services } = useZeroconf({ type: 'ipp' })

  return services.map(service => (
    <Text key={service.name}>
      {service.name} {service.ipv4[0]}:{service.port}
    </Text>
  ))
}
```

Without React:

```javascript
import Zeroconf from 'react-native-zeroconf'

const zeroconf = new Zeroconf()
zeroconf.on('resolved', service => console.log(service.name, service.host, service.port))
zeroconf.scan({ type: 'http' })
```

## Documentation

- **Getting started**: [Installation](https://zeroconf.balthazar.dev/Installation), [Permissions and Setup](https://zeroconf.balthazar.dev/Permissions-and-Setup)
- **Guides**: [Scanning](https://zeroconf.balthazar.dev/Scanning), [Publishing](https://zeroconf.balthazar.dev/Publishing), [React Integration](https://zeroconf.balthazar.dev/React-Integration), [Error Handling](https://zeroconf.balthazar.dev/Error-Handling)
- **Reference**: [API Reference](https://zeroconf.balthazar.dev/API-Reference), [Errors](https://zeroconf.balthazar.dev/Errors), [Platform Support](https://zeroconf.balthazar.dev/Platform-Support)
- **Android**: [Android Implementations](https://zeroconf.balthazar.dev/Android-Implementations), [Android Emulator](https://zeroconf.balthazar.dev/Android-Emulator)
- **Help**: [Troubleshooting and FAQ](https://zeroconf.balthazar.dev/Troubleshooting-and-FAQ), [Migration Guide](https://zeroconf.balthazar.dev/Migration-Guide)
- **Project**: [Contributing](https://zeroconf.balthazar.dev/Contributing), [example app](./example)

## License

MIT, see [LICENSE](LICENSE). Includes [RxDNSSD](https://github.com/discord/RxDNSSD) (Apache 2.0, see [NOTICE](NOTICE)) and Apple's mDNSResponder (mostly Apache 2.0, see its [LICENSE](android/src/main/jni/mdnsresponder/LICENSE)).
