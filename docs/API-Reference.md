# API Reference

```javascript
import Zeroconf, { ImplType } from 'react-native-zeroconf'
const zeroconf = new Zeroconf()
```

`Zeroconf` extends the [`events`](https://www.npmjs.com/package/events) `EventEmitter` (`on`, `once`, `off`, `removeListener`, `removeAllListeners`, `listenerCount`). The constructor throws if the native module is missing (for example in Expo Go).

**Contents:** [Methods](#methods) | [`useZeroconf`](#usezeroconfoptions) | [Multiple scans](#multiple-scans) | [Events](#events) | [Service object](#service-object) | [Errors](#errors) | [TypeScript types](#typescript-types)

## Methods

| Method | Returns | Summary |
| --- | --- | --- |
| [`scan(options?)`](#scanoptions) | `void` | Start browsing for a service type |
| [`stop(implType?)`](#stopimpltype) | `void` | Stop this instance's scan |
| [`getServices()`](#getservices) | `Record<string, Service>` | Services of the current scan, by name |
| [`publishService(options)`](#publishserviceoptions) | `Promise<PublishedService>` | Advertise a service |
| [`unpublishService(name, implType?)`](#unpublishservicename-impltype) | `Promise` | Withdraw a published service |
| [`checkLocalNetworkAccess(options?)`](#checklocalnetworkaccessoptions) | `Promise<'granted' \| 'denied' \| 'unknown'>` | iOS Local Network permission |
| [`subscribe(event, listener)`](#subscribeevent-listener) | `() => void` | Add a listener, get a function removing it |
| [`addDeviceListeners()`](#adddevicelisteners--removedevicelisteners) | `void` | Attach native event listeners (done by the constructor) |
| [`removeDeviceListeners()`](#adddevicelisteners--removedevicelisteners) | `void` | Detach native event listeners |

### `scan(options?)`

Starts browsing. Clears the list returned by `getServices()` and emits `update`. Each instance runs its own scan: calling `scan()` again on the same instance replaces that instance's scan, and other instances can scan at the same time (see [Multiple scans](#multiple-scans)).

| Option | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `type` | `string` | `'http'` | all | Service type without underscore, e.g. `'http'`, `'ipp'`, `'pdl-datastream'` |
| `protocol` | `string` | `'tcp'` | all | `'tcp'` or `'udp'` |
| `domain` | `string` | `'local.'` | all | Domain to browse |
| `implType` | `'NSD' \| 'DNSSD'` | `'NSD'` | Android | Discovery backend, see [Android Implementations](Android-Implementations) |
| `resolveTimeout` | `number` | `5` | iOS | Seconds to try resolving each service. A timeout is retried once, then reported as an `error` with code `'TIMEOUT'` |

```javascript
zeroconf.scan()
zeroconf.scan({ type: 'ipp', implType: ImplType.DNSSD, resolveTimeout: 10 })
```

> Deprecated: `scan(type, protocol, domain, implType)` still works.

### `stop(implType?)`

Stops this instance's scan and emits `stop`. Scans of other instances keep running.

| Parameter | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `implType` | `'NSD' \| 'DNSSD'` | implementation of the last `scan()` | Android | Which backend to stop |

### `getServices()`

Returns the services of this instance's scan, keyed by name. Services that are found but not yet resolved only contain `name`.

### `publishService(options)`

Advertises a service. Resolves with the service once it is advertised, rejects with a [`ZeroconfError`](#errors). The `published` event is also emitted.

| Option | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `type` | `string` | required | all | Service type without underscore |
| `protocol` | `string` | required | all | `'tcp'` or `'udp'` |
| `domain` | `string` | `'local.'` | all | Domain |
| `name` | `string` | required | all | Requested service name |
| `port` | `number` | required | all | Port |
| `txt` | `object \| [key, value][]` | `{}` | all | TXT record. Pairs keep an explicit order, see [TXT records](Publishing#txt-records) |
| `implType` | `'NSD' \| 'DNSSD'` | `'NSD'` | Android | Backend used to publish |

> **Use the resolved name.** The published name can differ from `name` when that name is already taken. Pass `service.name` to `unpublishService()`.

> Deprecated: `publishService(type, protocol, domain, name, port, txt, implType)` still works and also returns the promise.

### `unpublishService(name, implType?)`

Withdraws a published service. Resolves once it is no longer advertised and emits `unpublished`. Rejects with code `'NOT_PUBLISHED'` if no service is published under `name`.

| Parameter | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `name` | `string` | required | all | Name the service was published as |
| `implType` | `'NSD' \| 'DNSSD'` | backend the service was published with | Android | Which backend to use |

### `checkLocalNetworkAccess(options?)`

**iOS.** Resolves `'granted'`, `'denied'` or `'unknown'`. iOS has no API to read the Local Network permission, so this advertises a temporary service and browses for it. It can show the permission prompt if the user has not answered it yet. **Android** always resolves `'unknown'`.

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `type` | `string` | first entry of `NSBonjourServices` | Service type to test with, must be declared in `NSBonjourServices` |
| `protocol` | `string` | `'tcp'` | Protocol |
| `timeout` | `number` | `5` | Seconds before concluding `'denied'` or `'unknown'` |

Rejects with code `'MISSING_BONJOUR_SERVICES'` when no type is given and `NSBonjourServices` is empty. See [Permissions and Setup](Permissions-and-Setup#the-local-network-prompt).

### `subscribe(event, listener)`

Adds an event listener and returns a function that removes it. `on()` still works and returns the instance for chaining.

```javascript
const unsubscribe = zeroconf.subscribe('resolved', service => console.log(service.name))
unsubscribe()
```

### `addDeviceListeners()` / `removeDeviceListeners()`

The constructor calls `addDeviceListeners()`. Call `removeDeviceListeners()` when you are done with an instance (for example when a component unmounts) so it stops receiving native events. Calling `addDeviceListeners()` twice emits an `error`.

## `useZeroconf(options?)`

React hook that scans while the component is mounted. Takes the `scan()` options plus:

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `enabled` | boolean | `true` | Scan only while `true` |

Returns:

| Field | Type | Description |
| --- | --- | --- |
| `services` | `Service[]` | Resolved services, updated as they are found, resolved or removed |
| `isScanning` | boolean | Whether a scan is running |
| `error` | `ZeroconfError \| null` | Last error, reset when a new scan starts |
| `stop()` | function | Stops scanning |
| `restart()` | function | Clears the services and scans again |

It scans again when the options change, and stops and removes its listeners on unmount. Each hook runs its own scan, so several hooks can scan different types at once.

## Multiple scans

Each `Zeroconf` instance runs its own scan, and several can run at once.

| Events | Delivered to |
| --- | --- |
| Scan events (`start`, `stop`, `found`, `resolved`, `remove`, `update`) and scan errors | The instance that started the scan |
| Same, for an instance that never called `scan()` | Every scan's events (backwards compatible) |
| `published`, `unpublished`, errors not tied to a scan | Every instance |

On Android, `NSD` and `DNSSD` scans can run concurrently. On iOS, every scanned type must be in `NSBonjourServices`. See [Scanning](Scanning#multiple-scans).

## Events

| Event | Payload | When |
| --- | --- | --- |
| `start` | none | The scan started |
| `stop` | none | The scan stopped |
| `found` | `name: string` | A service appeared (not resolved yet) |
| `resolved` | `Service` | A service was resolved. On Android 14+ with `NSD`, it can fire again when addresses or TXT records change |
| `remove` | `name: string` | A service left the network |
| `update` | none | The list returned by `getServices()` changed |
| `error` | `ZeroconfError` | A scan, resolve or publish error |
| `published` | `PublishedService` | A service was advertised |
| `unpublished` | `PublishedService` | A service was withdrawn |

> Native errors are only forwarded when at least one `error` listener is registered.

## Service object

```typescript
interface Service {
  name: string        // 'Xerox Printer'
  fullName: string    // full DNS-SD name
  host: string        // 'XeroxPrinter.local.'
  port: number        // 8080
  addresses: string[] // IPv4 first, then IPv6
  ipv4: string[]
  ipv6: string[]
  txt: Record<string, string>
}
```

`PublishedService` (payload of `publishService()`, `published` and `unpublished`) has the same fields without `ipv4` and `ipv6`.

| Field | Notes |
| --- | --- |
| `host` | On Android with `NSD`, the mDNS host name on Android 16+, usually the IP address before. `DNSSD` gives the host name |
| `addresses` | Android `NSD` returns all addresses on Android 14+, a single one before |
| `txt` | Values decoded as UTF-8 strings |

## Errors

Errors from the `error` event and from promise rejections are `Error` objects with extra fields:

| Field | Type | Description |
| --- | --- | --- |
| `message` | `string` | Readable description |
| `code` | `number \| string` | Numeric platform code, or a string code from the library |
| `domain` | `string` | Where `code` comes from (table below) |
| `serviceName` | `string?` | The service concerned, when there is one |

### Domains

| `domain` | Platform | `code` |
| --- | --- | --- |
| `'DNSSD'` | iOS | `DNSServiceErrorType` from Apple's dns_sd API (negative number) |
| `'DNSSD'` | Android (`implType: 'DNSSD'`) | `DNSServiceErrorType` from the embedded mDNSResponder |
| `'NsdManager'` | Android (`implType: 'NSD'`) | `NsdManager` failure code |
| `'RNZeroconf'` | all | String code from the library |

### Common codes

| Domain | Code | Meaning |
| --- | --- | --- |
| `DNSSD` | `-65555` | `kDNSServiceErr_NoAuth`. iOS: the service type is missing from `NSBonjourServices` ("not authorized, add the service type to NSBonjourServices in Info.plist") |
| `DNSSD` | `-65570` | `kDNSServiceErr_PolicyDenied`. iOS: Local Network access denied |
| `DNSSD` | `-65548` | `kDNSServiceErr_NameConflict`, name already in use |
| `DNSSD` | `-65540` | `kDNSServiceErr_BadParam` |
| `DNSSD` | `-65554` | `kDNSServiceErr_NoSuchRecord` |
| `DNSSD` | `-65563` | `kDNSServiceErr_ServiceNotRunning`, mDNS daemon not running |
| `DNSSD` | `-65568` | `kDNSServiceErr_Timeout` |
| `NsdManager` | `0` | `FAILURE_INTERNAL_ERROR` |
| `NsdManager` | `3` | `FAILURE_ALREADY_ACTIVE` |
| `NsdManager` | `4` | `FAILURE_MAX_LIMIT`, too many requests |
| `NsdManager` | `5` | `FAILURE_OPERATION_NOT_RUNNING` (API 34+) |
| `NsdManager` | `6` | `FAILURE_BAD_PARAMETERS` (API 34+) |
| `RNZeroconf` | `'EXCEPTION'` | Unexpected native exception, see `message` |
| `RNZeroconf` | `'NOT_PUBLISHED'` | `unpublishService()` with a name that is not published |
| `RNZeroconf` | `'TIMEOUT'` | iOS: a service could not be resolved within `resolveTimeout` (after one retry) |
| `RNZeroconf` | `'TXT_ENTRY_TOO_LONG'` | iOS: a TXT entry over 255 bytes was left out |
| `RNZeroconf` | `'MISSING_BONJOUR_SERVICES'` | iOS: `checkLocalNetworkAccess()` has no type to test with |

```javascript
zeroconf.on('error', error => {
  if (error.domain === 'DNSSD' && error.code === -65555) {
    // iOS: add the service type to NSBonjourServices
  } else if (error.domain === 'DNSSD' && error.code === -65570) {
    // iOS: Local Network access denied, send the user to Settings
  }
})
```

## TypeScript types

Exported from `react-native-zeroconf`:

| Type | Description |
| --- | --- |
| `ImplType` | `'NSD' \| 'DNSSD'` (also a value: `ImplType.NSD`, `ImplType.DNSSD`) |
| `Service` | Resolved service |
| `PublishedService` | `Service` without `ipv4` / `ipv6` |
| `ScanOptions` | Options of `scan()` |
| `PublishOptions` | Options of `publishService()` |
| `LocalNetworkAccessOptions` | Options of `checkLocalNetworkAccess()` |
| `TxtRecord` | Object or `[key, value]` pairs |
| `ZeroconfError` | Error with `code`, `domain`, `serviceName` |
| `ZeroconfEvents` | Event name to listener signature map |
| `UseZeroconfOptions` | Options of `useZeroconf()` |
| `UseZeroconfResult` | Return value of `useZeroconf()` |

Listeners are typed by event name:

```typescript
zeroconf.on('resolved', (service: Service) => {})
zeroconf.on('remove', (name: string) => {})
```
