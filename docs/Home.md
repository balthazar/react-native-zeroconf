# react-native-zeroconf

Discover and publish network services on the local network with Zeroconf (Bonjour, Avahi, mDNS) from React Native, on iOS, macOS, tvOS, Android and Windows.

```javascript
import Zeroconf from 'react-native-zeroconf'

const zeroconf = new Zeroconf()
zeroconf.on('resolved', service => console.log(service.name, service.addresses, service.port))
zeroconf.scan({ type: 'http' })
```

> **Documentation for 0.17.** Upgrading from an earlier version? Read the [Migration Guide](Migration-Guide).

## Start here

| If you want to... | Read |
| --- | --- |
| Add the library to a React Native or Expo app | [Installation](Installation) |
| Configure `Info.plist`, Local Network access and Android permissions | [Permissions and Setup](Permissions-and-Setup) |
| Scan for services and read what they resolve to | [Scanning](Scanning) |
| Advertise your own services and TXT records | [Publishing](Publishing) |
| Use the `useZeroconf` hook and clean up in components | [React Integration](React-Integration) |
| Tell errors apart and react to them | [Error Handling](Error-Handling) |
| Look up a method, option or event | [API Reference](API-Reference) |
| Look up an error code or domain | [Errors](Errors) |
| Choose between `NSD` and `DNSSD` on Android | [Android Implementations](Android-Implementations) |
| Test on the Android emulator | [Android Emulator](Android-Emulator) |
| Fix "nothing is found", `-65555` / `-65570`, Expo Go crashes... | [Troubleshooting and FAQ](Troubleshooting-and-FAQ) |
| Upgrade from an earlier version | [Migration Guide](Migration-Guide) |
| Check what each platform supports | [Platform Support](Platform-Support) |
| Build the library and the example app | [Contributing](Contributing) |

## At a glance

- **React hook**: `useZeroconf()` scans while a component is mounted and returns the resolved services.
- **Discovery**: `scan()` emits `found`, then `resolved` with host, port, addresses (IPv4 first) and TXT records.
- **Multiple scans**: each `Zeroconf` instance (and each `useZeroconf` hook) runs its own scan, so several service types can be browsed at once.
- **Service types**: `scanServiceTypes()` lists the service types advertised on the network (Android, or iOS with the multicast entitlement).
- **Live updates**: `resolved` fires again when a service's addresses or TXT record change.
- **Subtypes and interfaces**: scan or publish subtypes, and pick a network interface.
- **Single services**: `resolveService()` reaches a known service by name, `updateService()` changes a published TXT record.
- **Publishing**: `publishService()` returns a promise with the service as advertised (its name can be renamed on conflict).
- **Structured errors**: every error carries `code`, `domain` and, when relevant, `serviceName`.
- **Local Network permission**: `checkLocalNetworkAccess()` checks iOS Local Network access and the Android 17 `ACCESS_LOCAL_NETWORK` permission.
- **Two Android backends**: the system `NsdManager` (`NSD`, default) or an embedded mDNSResponder (`DNSSD`).
- **TypeScript**: type definitions ship with the package.

## Links

- [Repository and README](https://github.com/balthazar/react-native-zeroconf)
- [Issues](https://github.com/balthazar/react-native-zeroconf/issues)
- [npm](https://www.npmjs.com/package/react-native-zeroconf)
