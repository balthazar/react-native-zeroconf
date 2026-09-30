# Migration Guide: 0.15 to 0.16

0.16 keeps the old call forms working, so most apps upgrade without code changes. The changes below are what to review, starting with the ones that can change behavior.

| Change | Action needed? |
| --- | --- |
| [Errors are structured](#structured-errors) | **Yes**, if you parse error messages |
| [`publishService` / `unpublishService` return promises](#publishing-returns-promises) | Recommended, use the resolved name |
| [Options objects for `scan` / `publishService`](#options-objects) | Recommended, positional forms are deprecated |
| [Android 14+ `NSD` follows services](#android-14-nsd-can-emit-resolved-more-than-once) | Check your `resolved` handler |
| [RxJava no longer exposed on Android](#rxjava-is-no-longer-exposed-on-android) | Only if your app used it without declaring it |
| [New: multiple scans, `useZeroconf`, `subscribe`, `resolveTimeout`, `checkLocalNetworkAccess`](#new-features) | Optional |

## Structured errors

`error` events and promise rejections now receive an `Error` with `message`, `code`, `domain` and `serviceName`. In 0.15, iOS errors were a plain `Error` whose message was a stringified dictionary (e.g. containing `NSNetServicesErrorCode = "-72008"`), and Android errors were plain strings. Stop parsing `error.message` and switch on `domain` and `code` instead. See the [error tables](Errors).

The iOS implementation also moved from `NSNetService` to Apple's dns_sd C API, so iOS codes changed:

| 0.15 (in the message) | 0.16 |
| --- | --- |
| `NSNetServicesErrorCode = "-72008"`, missing `NSBonjourServices` entry | `domain: 'DNSSD'`, `code: -65555` (`kDNSServiceErr_NoAuth`) |
| Local Network access denied | `domain: 'DNSSD'`, `code: -65570` (`kDNSServiceErr_PolicyDenied`) |
| `NSNetServicesErrorCode = "-72007"`, timeout | `domain: 'RNZeroconf'`, `code: 'TIMEOUT'` for resolves (after one automatic retry) |

```javascript
// Before
if (error.message.includes('-72008')) { /* ... */ }

// After
if (error.domain === 'DNSSD' && error.code === -65555) { /* ... */ }
```

```javascript
zeroconf.on('error', error => {
  console.warn(`[${error.domain}] ${error.code}: ${error.message}`, error.serviceName)
})
```

## Publishing returns promises

```javascript
// Before
zeroconf.on('published', service => { /* ... */ })
zeroconf.publishService('http', 'tcp', 'local.', 'MyServer', 8080, { path: '/' })
// later
zeroconf.unpublishService('MyServer')

// After
const service = await zeroconf.publishService({ type: 'http', protocol: 'tcp', name: 'MyServer', port: 8080, txt: { path: '/' } })
// later
await zeroconf.unpublishService(service.name)
```

- The resolved `service.name` can differ from the requested name when that name is taken. Unpublish with the resolved name.
- `unpublishService()` rejects with `'NOT_PUBLISHED'` for unknown names.
- `published` and `unpublished` events are still emitted.
- You do not need to `await` or `.catch()` the promises: an un-awaited rejection does not trigger an unhandled rejection warning (errors are also emitted as `error` events).

## Options objects

```javascript
// Deprecated, still works
zeroconf.scan('http', 'tcp', 'local.', 'DNSSD')
zeroconf.publishService('http', 'tcp', 'local.', 'MyServer', 8080, {}, 'DNSSD')

// 0.16
zeroconf.scan({ type: 'http', implType: 'DNSSD' })
zeroconf.publishService({ type: 'http', protocol: 'tcp', name: 'MyServer', port: 8080, implType: 'DNSSD' })
```

Omitted or `undefined` options take their defaults, so you only pass what differs.

## Android 14+: NSD can emit `resolved` more than once

With `implType: 'NSD'` on Android 14+, found services are followed with `NsdManager.registerServiceInfoCallback` instead of one-shot resolves. Several services resolve in parallel, and a service is emitted as `resolved` again when its addresses or TXT records change. Make your `resolved` handler idempotent (update by `service.name` rather than appending).

## RxJava is no longer exposed on Android

The Android module now declares RxJava as an `implementation` dependency. If your app code used RxJava classes without declaring RxJava itself, add the dependency to your app.

## Coming from 0.14 or earlier

- `stop()` and `unpublishService()` default to the implementation that was used by the last scan, or by the publish, instead of always `NSD`.
- TypeScript types are bundled: uninstall `@types/react-native-zeroconf`.
- `addresses` lists IPv4 first, with new `ipv4` and `ipv6` arrays.
- TXT records can be given as `[key, value]` pairs and are published in the given order (iOS and `DNSSD`).

## New features

- **Multiple scans**: each `Zeroconf` instance runs its own scan and receives only its own scan events, so several types can be browsed at once. An instance that never called `scan()` still receives every scan's events, so existing code keeps working. See [Multiple scans](Scanning#multiple-scans).
- `scan({ resolveTimeout })` (iOS): seconds to resolve a service, default `5`.
- `useZeroconf()` React hook: scans while mounted and returns `{ services, isScanning, error, stop, restart }`. See [React Integration](React-Integration#the-usezeroconf-hook).
- `subscribe(event, listener)` returns a function removing the listener.
- `checkLocalNetworkAccess()` (iOS): `'granted' | 'denied' | 'unknown'`. See [Permissions and Setup](Permissions-and-Setup#the-local-network-prompt).
