# Platform Support

| Platform | Minimum | Implementation |
| --- | --- | --- |
| iOS | React Native's minimum (13.4 on React Native versions that don't define one) | Apple dns_sd C API |
| macOS (react-native-macos) | 10.15 | Apple dns_sd C API |
| tvOS | 13.4 | Apple dns_sd C API |
| Android | API 21 (Android 5.0) | `NsdManager` (`NSD`) or embedded mDNSResponder (`DNSSD`) |
| Expo | Development builds only (tested with SDK 54) | Same as the native platform |
| Expo Go | Not supported | |

## Features

| Feature | iOS | Android `NSD` | Android `DNSSD` |
| --- | --- | --- | --- |
| Discovery | ✅ | ✅ | ✅ |
| Publishing | ✅ | ✅ | ✅ |
| TXT records | ✅ | ✅ | ✅ |
| TXT order preserved when publishing | ✅ | System dependent | ✅ |
| IPv4 and IPv6 addresses | ✅ | All on Android 14+, one before | ✅ |
| mDNS host name in `host` | ✅ | Android 16+ | ✅ |
| Updates re-emitted as `resolved` | | Android 14+ | |
| `resolveTimeout` | ✅ | | |
| `checkLocalNetworkAccess()` | ✅ | Resolves `'unknown'` | Resolves `'unknown'` |
| `scanServiceTypes()` | With the multicast entitlement, see [Listing service types](Scanning#listing-service-types) | ✅ Other devices' types | ✅ |
| Error domain | `DNSSD`, `RNZeroconf` | `NsdManager`, `RNZeroconf` | `DNSSD`, `RNZeroconf` |

## Architecture

| | Supported |
| --- | --- |
| New Architecture | ✅ (tested with Expo SDK 54) |
| TypeScript | ✅ bundled types |
| Android 16 KB page size | ✅ native libraries aligned |

<!-- TODO: confirm macOS and tvOS feature parity (checkLocalNetworkAccess, Local Network prompt) before publishing. -->
