# Platform Support

| Platform | Minimum | Implementation |
| --- | --- | --- |
| iOS | 13.4 or React Native's minimum | Apple dns_sd C API |
| macOS | 10.15 | Apple dns_sd C API, with react-native-macos |
| tvOS | 13.4 | Apple dns_sd C API |
| Android | API 21 (Android 5.0) | `NsdManager` or embedded mDNSResponder |
| Windows | Windows 10 | `windns.h`, with react-native-windows |
| Expo | Development builds, SDK 54 tested | Same as the native platform |
| Expo Go | ❌ | |

## Features

✅ supported, 🟡 partial or conditional (hover or tap it for details), ❌ not supported.

| Feature | iOS | Android `NSD` | Android `DNSSD` | Windows |
| --- | :---: | :---: | :---: | :---: |
| Discovery | ✅ | ✅ | ✅ | ✅ |
| Publishing | ✅ | ✅ | ✅ | ✅ |
| TXT records | ✅ | ✅ | ✅ | ✅ |
| TXT order kept when publishing | ✅ | <span title="Depends on the Android version and device">🟡</span> | ✅ | ✅ |
| IPv4 and IPv6 addresses | ✅ | <span title="All addresses on Android 14+, one before">🟡</span> | ✅ | <span title="One IPv4 and one IPv6 address">🟡</span> |
| mDNS host name in `host` | ✅ | <span title="Android 16+">🟡</span> | ✅ | ✅ |
| Updates re-emitted as `resolved` | ✅ | <span title="Android 14+">🟡</span> | ❌ | <span title="TXT record, host and port">🟡</span> |
| `resolveTimeout` | ✅ | <span title="Android 13 and earlier can't cancel a resolve to retry it, the error comes after twice the timeout">✅</span> | ✅ | ✅ |
| `checkLocalNetworkAccess()` | ✅ | <span title="Checks the ACCESS_LOCAL_NETWORK permission of Android 17">✅</span> | <span title="Checks the ACCESS_LOCAL_NETWORK permission of Android 17">✅</span> | <span title="Resolves 'granted', there is nothing to grant">✅</span> |
| Subtypes (`subtype`, `subtypes`) | ✅ | ✅ | ✅ | <span title="Scanning only, publishing subtypes rejects 'UNSUPPORTED'">🟡</span> |
| `networkInterface` | ✅ | <span title="Android 13+">🟡</span> | ✅ | <span title="Adapter name ('Wi-Fi') or interface index">✅</span> |
| `updateService()` | ✅ | <span title="Publishes the service again instead of updating it in place">✅</span> | ✅ | <span title="Publishes the service again instead of updating it in place">✅</span> |
| `resolveService()` | ✅ | ✅ | ✅ | ✅ |
| [`scanServiceTypes()`](Scanning#listing-service-types) | <span title="Requires Apple's multicast entitlement, see Listing service types on the Scanning page">🟡</span> | <span title="The types published by other apps on the same device are not listed">🟡</span> | ✅ | <span title="The types published by other apps on the same device are not listed">🟡</span> |

Errors use the `RNZeroconf` domain, plus `DNSSD` on iOS and Android `DNSSD`, `NsdManager` on Android `NSD`, and `Windows` on Windows. See [Errors](Errors).

> **Windows** is tested in CI: the native code against a second mDNS implementation (subtypes, live updates and service types included), and a react-native-windows app running the JavaScript API.

## Architecture

| | Supported |
| --- | --- |
| New Architecture | ✅ tested with Expo SDK 54 |
| TypeScript | ✅ bundled types |
| Android 16 KB page size | ✅ native libraries aligned |

<!-- TODO: confirm macOS and tvOS feature parity (checkLocalNetworkAccess, Local Network prompt) before publishing. -->
