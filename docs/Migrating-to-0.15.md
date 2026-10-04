# Migrating to 0.15

From 0.14 and earlier to 0.15. Upgrading across several versions? Follow the [Migration Guide](Migration-Guide) from your version up.

0.15 (0.15.0 to 0.15.2) is the first release after 0.13.8 that builds on Android: 0.14.0's Android build fails, skip it. The JavaScript API is the same, the changes below are behavior and build requirements.

| Change | Action needed? |
| --- | --- |
| [Android `DNSSD` reports removals and the host name](#android-dnssd-reports-removals-and-the-host-name) | **Yes**, if you use `implType: 'DNSSD'` |
| [`stop()` and `unpublishService()` use the implementation in use](#stop-and-unpublishservice-use-the-implementation-in-use) | Check, if you mix `NSD` and `DNSSD` |
| [Build requirements](#build-requirements) | **Yes**, Android SDK 34 and a rebuild |
| [Publishing failures are `error` events](#publishing-failures-are-error-events) | Recommended, listen to `error` |
| [Teardown on reload](#teardown-on-reload) | None |
| [New: addresses, TXT order, bundled types](#new-in-015) | Optional |

## Android `DNSSD` reports removals and the host name

With `implType: 'DNSSD'`:

- A service leaving the network emits `remove`. Before, it was emitted as `resolved` again.
- `found` is emitted before the service is resolved, as with `NSD` and on iOS.
- `host` is the service's host name (`MyPrinter.local.`). Before, it was the service name.
- One service failing to resolve no longer stops the scan, and `stop` is emitted if the scan ends with an error.

## `stop()` and `unpublishService()` use the implementation in use

Without an `implType` argument, `stop()` stops the implementation of the last `scan()`, and `unpublishService()` the one the service was published with. Before, both defaulted to `NSD`, so a `DNSSD` scan kept running after `stop()`. Passing `implType` explicitly still works.

## Build requirements

- **Android SDK 34**: the app must compile against Android SDK 34 or newer (the default from React Native 0.73).
- **Rebuild the app**: the native method signatures changed.
- **`commons-lang3`** is no longer a dependency of the library. If your app used it without declaring it, add it to your app.
- **Android permissions** are declared by the library and merged into your app. You can remove the ones you added for it (`INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE`, `CHANGE_WIFI_MULTICAST_STATE`) if nothing else needs them.

## Publishing failures are `error` events

Failures to publish or unpublish a service are emitted as `error` events, on `NSD` and `DNSSD`. Add an `error` listener to know about them.

## Teardown on reload

When React Native reloads or tears down the app, scans are stopped and published services are unpublished, on iOS and Android. Before, Android scans kept running after a reload on current React Native versions.

## New in 0.15

- `addresses` lists IPv4 addresses first, and resolved services have `ipv4` and `ipv6` arrays.
- On Android 14+, `NSD` returns all of a service's addresses, and the mDNS host name on Android 16+.
- TXT records can be given as `[key, value]` pairs, and are published in the given order on iOS and with `DNSSD`.
- TypeScript types are bundled: uninstall `@types/react-native-zeroconf`.
- `publishService()` no longer modifies the `txt` object you pass.
- iOS: a resolve that times out is retried once, TXT values are decoded as UTF-8.
- A clear error is thrown when the native module is missing, for example in Expo Go (use a development build).
