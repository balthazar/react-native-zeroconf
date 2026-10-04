# Migrating to 1.0

> **Release candidate:** install it with `npm install react-native-zeroconf@next`, 0.17 stays the default.

From 0.17 to 1.0. Upgrading across several versions? Follow the [Migration Guide](Migration-Guide) from your version up.

1.0 is one C++ TurboModule on every platform, for the New Architecture. The JavaScript API is the one documented for 0.17, minus the positional forms deprecated since 0.16. Apps still on the Legacy Architecture stay on 0.17.x, which keeps getting bug fixes.

| Change | Action needed? |
| --- | --- |
| React Native 0.82 or later, the New Architecture required (Expo SDK 55 and later) | Upgrade React Native, or stay on 0.17.x |
| `scan(type, protocol, domain, implType)` removed, it throws a `TypeError` | `scan({ type, protocol, domain, implType })` |
| `publishService(type, protocol, domain, name, port, txt, implType)` removed, it throws a `TypeError` | `publishService({ type, protocol, domain, name, port, txt, implType })` |
| The native module is `Zeroconf`, a TurboModule: `NativeModules.RNZeroconf` and its `RNZeroconf*` device events are gone | Only if you used them directly instead of the JavaScript API |
| The package exposes its root only (`exports`), built to `lib/` | Import from `react-native-zeroconf`, not from files inside it |
| Android: the module is C++, built by the app through autolinking; the library no longer sets `ndkVersion` | None, New Architecture apps already build C++ with the NDK |
| Android `DNSSD`: `resolved` fires again when a service's addresses or TXT record change, as on iOS | Update your list by `service.name` rather than appending |
| Android: React Native's minimum SDK (API 24 in React Native 0.86) | None for apps on React Native 0.82 or later |

Rebuild the app after upgrading: `pod install` on iOS, a clean build on Android and Windows.
