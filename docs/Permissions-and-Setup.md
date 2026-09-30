# Permissions and Setup

## iOS

Since iOS 14, apps must declare the Bonjour service types they browse or publish, and the user must grant **Local Network** access.

### Info.plist

```xml
<key>NSBonjourServices</key>
<array>
    <string>_http._tcp.</string>
    <string>_printer._tcp.</string>
    <!-- One entry per service type you scan or publish -->
</array>
<key>NSLocalNetworkUsageDescription</key>
<string>This app uses the local network to discover printers and other devices.</string>
```

| Key | Purpose |
| --- | --- |
| `NSBonjourServices` | Every type you pass to `scan()` or `publishService()`, written as `_<type>._<protocol>`. A type missing here fails with a `DNSSD` error `-65555` (`kDNSServiceErr_NoAuth`). With several scans, list every scanned type. |
| `NSLocalNetworkUsageDescription` | Text shown in the Local Network permission prompt. |

> **Expo:** put the same keys under `expo.ios.infoPlist` in `app.json` (see [Installation](Installation#expo)).

### The Local Network prompt

iOS shows the prompt the first time the app uses the local network (for example your first `scan()`). If the user declines, browsing and publishing fail with `-65570` until they enable access in **Settings > Privacy & Security > Local Network**.

iOS has no API to read this permission. The library provides `checkLocalNetworkAccess()`, which advertises a temporary service and browses for it:

```javascript
const access = await zeroconf.checkLocalNetworkAccess({ timeout: 5 })

switch (access) {
  case 'granted':
    zeroconf.scan({ type: 'http' })
    break
  case 'denied':
    // Explain why and point the user to Settings
    break
  case 'unknown':
    // Could not tell (for example no answer before the timeout)
    break
}
```

- It uses the first entry of `NSBonjourServices` unless you pass `type` (and `protocol`). The type must be declared, otherwise it rejects with code `'MISSING_BONJOUR_SERVICES'`.
- It can trigger the permission prompt if the user has not answered it yet, so call it at a moment where the prompt makes sense.
- On Android it always resolves `'unknown'`.

### iOS errors in short

| Error | Cause | Fix |
| --- | --- | --- |
| `DNSSD` `-65555` (`kDNSServiceErr_NoAuth`) | Service type not in `NSBonjourServices` | Add `_<type>._<protocol>` and rebuild |
| `DNSSD` `-65570` (`kDNSServiceErr_PolicyDenied`) | User denied Local Network access | Confirm with `checkLocalNetworkAccess()`, send the user to Settings |

## Android

The library manifest declares these permissions and they are merged into your app automatically. You do not need to add them yourself:

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
```

None of them is a runtime (dangerous) permission, so there is nothing to request at runtime.

> The Android emulator does not pass multicast traffic by default, so discovery usually finds nothing there. Use a real device, or see [Android Emulator](Android-Emulator).

## macOS and tvOS

The same `Info.plist` keys apply. <!-- TODO: confirm whether macOS apps also need the App Sandbox network entitlements (com.apple.security.network.client/server) for discovery and publishing. -->
