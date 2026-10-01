# Platform Support

| Platform | Minimum | Implementation |
| --- | --- | --- |
| iOS | React Native's minimum (13.4 on React Native versions that don't define one) | Apple dns_sd C API |
| macOS (react-native-macos) | 10.15 | Apple dns_sd C API |
| tvOS | 13.4 | Apple dns_sd C API |
| Android | API 21 (Android 5.0) | `NsdManager` (`NSD`) or embedded mDNSResponder (`DNSSD`) |
| Windows (react-native-windows) | Windows 10 | DNS-SD functions of `windns.h` |
| Expo | Development builds only (tested with SDK 54) | Same as the native platform |
| Expo Go | Not supported | |

## Features

| Feature | iOS | Android `NSD` | Android `DNSSD` | Windows |
| --- | --- | --- | --- | --- |
| Discovery | ✅ | ✅ | ✅ | ✅ |
| Publishing | ✅ | ✅ | ✅ | ✅ |
| TXT records | ✅ | ✅ | ✅ | ✅ |
| TXT order preserved when publishing | ✅ | System dependent | ✅ | ✅ |
| IPv4 and IPv6 addresses | ✅ | All on Android 14+, one before | ✅ | One of each |
| mDNS host name in `host` | ✅ | Android 16+ | ✅ | ✅ |
| Updates re-emitted as `resolved` | ✅ | Android 14+ | | ✅ TXT, host and port |
| `resolveTimeout` | ✅ | | | |
| `checkLocalNetworkAccess()` | ✅ | ✅ `ACCESS_LOCAL_NETWORK` (Android 17) | ✅ `ACCESS_LOCAL_NETWORK` (Android 17) | Resolves `'granted'` (nothing to grant) |
| Subtypes (`subtype`, `subtypes`) | ✅ | ✅ | ✅ | Scanning only, publishing rejects `'UNSUPPORTED'` |
| `networkInterface` | ✅ | Android 13+ | ✅ | ✅ Adapter name (`'Wi-Fi'`) or interface index |
| `updateService()` | ✅ In place | ✅ Publishes again | ✅ In place | ✅ Publishes again |
| `resolveService()` | ✅ | ✅ | ✅ | ✅ |
| `scanServiceTypes()` | With the multicast entitlement, see [Listing service types](Scanning#listing-service-types) | ✅ Except other apps' on the same phone | ✅ | ✅ Except other apps' on the same computer |
| Error domain | `DNSSD`, `RNZeroconf` | `NsdManager`, `RNZeroconf` | `DNSSD`, `RNZeroconf` | `Windows`, `RNZeroconf` |

> **Windows** is tested in CI: the native code against a second mDNS implementation (subtypes, live updates and service types included), and a react-native-windows app running the JavaScript API.

## Architecture

| | Supported |
| --- | --- |
| New Architecture | ✅ (tested with Expo SDK 54) |
| TypeScript | ✅ bundled types |
| Android 16 KB page size | ✅ native libraries aligned |

<!-- TODO: confirm macOS and tvOS feature parity (checkLocalNetworkAccess, Local Network prompt) before publishing. -->
