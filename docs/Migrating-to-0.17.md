# Migrating to 0.17

From 0.16 to 0.17. Upgrading across several versions? Follow the [Migration Guide](Migration-Guide) from your version up.

0.17 has no breaking change in the JavaScript API. Rebuild the app after upgrading (`pod install` on iOS), the native code changed.

| Change | Action needed? |
| --- | --- |
| Android: `checkLocalNetworkAccess()` resolves `'granted'` or `'denied'` instead of `'unknown'`, and can show the Android 17 permission prompt | Pass `request: false` to only check |
| iOS: `resolved` fires again when a service's addresses or TXT record change, as on Android 14+ with `NSD` | Update your list by `service.name` rather than appending |
| Android `DNSSD`: resolve and publish errors carry their `DNSSD` code (they were `RNZeroconf` `'EXCEPTION'`), published services report their port | Only if you matched `'EXCEPTION'` |
| Android `DNSSD` no longer uses RxJava | None |
| New: [Windows](Installation-Windows), [`scanServiceTypes()` / `useServiceTypes()`](Scanning#listing-service-types), [subtypes](Scanning#subtypes), [`networkInterface`](Scanning#choosing-a-network-interface), [`updateService()`](Publishing#updating-the-txt-record), [`resolveService()`](Scanning#resolving-a-single-service), the [Android 17 permission](Permissions-and-Setup#android-17-local-network-permission) | Optional |

## Patch releases

### 0.17.1

| Change | Action needed? |
| --- | --- |
| Android and Windows: `resolveTimeout` applies, as on iOS. A service that doesn't resolve in time is retried once, then reported as an `error` with code `'TIMEOUT'` (scans were silent before) | Only if you treat every `error` as fatal: a `TIMEOUT` names one service in `serviceName`, the scan goes on |
| Android 13 and earlier: `resolveService()` rejects with `'TIMEOUT'` after `timeout`, it could wait indefinitely before | None |

### 0.17.2 and 0.17.3

Fixes only, no action needed: on Android 13 and earlier a resolve that never answers no longer blocks the later ones (0.17.2), and Android `DNSSD` scans no longer crash the app (0.17.3). See the [releases](https://github.com/balthazar/react-native-zeroconf/releases).
