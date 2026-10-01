# Troubleshooting and FAQ

Start with the checklist, then find your symptom below.

## Checklist

1. Device and service are on the **same network and subnet** (no guest Wi-Fi, no client isolation, VPN off).
2. **iOS:** the type is in `NSBonjourServices` and Local Network access is granted (`checkLocalNetworkAccess()`).
3. **Android:** you test on a **real device**, not the emulator.
4. You registered an **`error` listener** (native errors are dropped without one).
5. **Android:** try `implType: 'DNSSD'`.
6. The app was **rebuilt** after installing or upgrading the library (`pod install` on iOS).

## Symptoms

### Native module not found (Expo Go)

`react-native-zeroconf: native module not found. Make sure the library is linked and the app rebuilt. Expo Go is not supported, use a development build instead.`

Older versions crashed with an error about `null` (for example reading a property of the native module). Both mean the native code is not in the binary:

- **Expo Go** cannot load third-party native modules. Use a development build: `npx expo run:ios`, `npx expo run:android` or EAS Build.
- **Bare app:** run `cd ios && pod install` and rebuild the app. A Metro reload is not enough.

### iOS: error `-65555` (`kDNSServiceErr_NoAuth`)

Message: "not authorized, add the service type to NSBonjourServices in Info.plist". The type you scan or publish is not declared.

Add `_<type>._<protocol>` (for example `_http._tcp`) to `NSBonjourServices` (in Expo: `expo.ios.infoPlist`) and rebuild. When running several scans, every scanned type must be listed. See [Permissions and Setup](Permissions-and-Setup).

### iOS: error `-65570` (`kDNSServiceErr_PolicyDenied`)

Message: "Local Network access denied". The user declined the Local Network prompt.

Call `checkLocalNetworkAccess()`. If it returns `'denied'`, ask the user to enable **Settings > Privacy & Security > Local Network** for your app. See [Permissions and Setup](Permissions-and-Setup#the-local-network-prompt).

### iOS: error message mentioning `-72008` (0.15 and older)

This is the 0.15 equivalent of `-65555`: the service type is missing from `NSBonjourServices`. 0.16 reports it as `domain: 'DNSSD'`, `code: -65555` (see [Migration Guide](Migration-Guide#structured-errors)).

### iOS: `found` but never `resolved`, or `TIMEOUT` errors

The device answers slowly. Increase `resolveTimeout`:

```javascript
zeroconf.scan({ type: 'ipp', resolveTimeout: 15 })
```

### Nothing is found on the Android emulator

The emulator does not support multicast by default, and mDNS needs multicast (`224.0.0.251:5353`). Use a real device, or the advanced setup in [Android Emulator](Android-Emulator).

### Android 17: `NsdManager` error `7`, or publishing fails with "Missing local network permission"

The app targets API 37, or declares `ACCESS_LOCAL_NETWORK` in its manifest, and the user hasn't granted it. Request it with `checkLocalNetworkAccess()` before scanning or publishing. See [Permissions and Setup](Permissions-and-Setup#android-17-local-network-permission).

### Android: discovery stops or is unreliable

`NsdManager` is known to stop silently, fail resolves, or be throttled by OEM battery optimizations. Try, in order:

1. `implType: 'DNSSD'`.
2. Stop scans in the background and restart them when the app is active:

```javascript
import { AppState } from 'react-native'

const subscription = AppState.addEventListener('change', state => {
  if (state === 'active') zeroconf.scan({ type: 'http', implType: 'DNSSD' })
  else zeroconf.stop()
})
// later: subscription.remove()
```

3. Wait about 500 ms between `stop()` and the next `scan()`.
4. Retry a scan that found nothing after a few seconds (avoid scans shorter than about 3 seconds).
5. Avoid calling `stop()` when no scan is running; track whether you are scanning.

Creating extra `Zeroconf` instances does not work around `NSD` issues: each instance runs its own scan on the same system service.

### Android: `host` is an IP address

With `NSD`, Android exposes the mDNS host name only on Android 16+. Use `implType: 'DNSSD'` if you need it on older versions. Connect with `service.ipv4[0]` either way.

### Android: only one address

`NSD` returns a single address before Android 14. `DNSSD` returns all resolved addresses.

### TXT records are in the wrong order

- Plain objects move integer-like keys (`'0'`, `'1'`) first. Pass `[key, value]` pairs instead.
- With `NSD` on Android the system decides the order. Use `DNSSD` if order matters.

### TXT records are empty

- Check the device actually publishes TXT records (for example with `dns-sd -L "<name>" _http._tcp` on a Mac).
- On iOS, entries over 255 bytes are dropped and reported with `'TXT_ENTRY_TOO_LONG'`.

### My published name changed

Another service already used it, so a new name was picked. Use the name `publishService()` resolves with.

### Services stay visible after my app is killed

A killed process cannot announce its departure. Other devices drop the service when its mDNS records expire. Services are unpublished automatically on a normal teardown or reload.

### Memory leaks or duplicate events

Every instance keeps receiving native events (its own scan's events, or all scan events if it never scanned) until you call `removeDeviceListeners()`. Clean up on unmount (see [React Integration](React-Integration#cleanup-in-react-components)).

## FAQ

**Does it work with the New Architecture?**
Yes, it is tested with Expo SDK 54 and the New Architecture.

**Can I run two scans at once?**
Yes. Each `Zeroconf` instance runs its own scan and receives only its own scan's events; each `useZeroconf` hook does the same. On Android, `NSD` and `DNSSD` scans can even run concurrently. On iOS, list every scanned type in `NSBonjourServices`. See [Multiple scans](Scanning#multiple-scans).

**Does it work on macOS or tvOS?**
The podspec supports macOS 10.15 and tvOS 13.4. See [Platform Support](Platform-Support).

**Do I need to request Android permissions at runtime?**
Not before Android 17. On Android 17, apps targeting API 37 (or declaring the permission) need `ACCESS_LOCAL_NETWORK`: request it with `checkLocalNetworkAccess()`, see [Permissions and Setup](Permissions-and-Setup#android-17-local-network-permission).

**Can I list every service type on the network (`_services._dns-sd._udp`)?**
Yes on Android, with `scanServiceTypes()` or `useServiceTypes()`. On iOS it requires the multicast entitlement from Apple. See [Listing service types](Scanning#listing-service-types).

**How do I reconnect to a device I found before?**
Resolve it by name with `resolveService()`, no scan needed. See [Resolving a single service](Scanning#resolving-a-single-service).

**Still stuck?** Open an [issue](https://github.com/balthazar/react-native-zeroconf/issues) with the platform, OS version, `implType`, the service type, and the `domain` and `code` of any error.
