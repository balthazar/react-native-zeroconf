# API Reference

```javascript
import Zeroconf, { ImplType } from 'react-native-zeroconf'
const zeroconf = new Zeroconf()
```

`Zeroconf` extends the [`events`](https://www.npmjs.com/package/events) `EventEmitter` (`on`, `once`, `off`, `removeListener`, `removeAllListeners`, `listenerCount`). The constructor throws if the native module is missing (for example in Expo Go).

**Contents:** [Methods](#methods) | [`useZeroconf`](#usezeroconfoptions) | [`useServiceTypes`](#useservicetypesoptions) | [Multiple scans](#multiple-scans) | [Events](#events) | [Service object](#service-object) | [TypeScript types](#typescript-types)

Error codes and domains are on the [Errors](Errors) page.

## Methods

| Method | Returns | Summary |
| --- | --- | --- |
| [`scan(options?)`](#scanoptions) | `void` | Start browsing for a service type |
| [`stop(implType?)`](#stopimpltype) | `void` | Stop this instance's scan |
| [`getServices()`](#getservices) | `Record<string, Service>` | Services of the current scan, by name |
| [`scanServiceTypes(options?)`](#scanservicetypesoptions) | `void` | List the service types advertised on the network |
| [`getServiceTypes()`](#getservicetypes) | `ServiceType[]` | Service types found by `scanServiceTypes()` |
| [`publishService(options)`](#publishserviceoptions) | `Promise<PublishedService>` | Advertise a service |
| [`unpublishService(name, implType?)`](#unpublishservicename-impltype) | `Promise` | Withdraw a published service |
| [`updateService(name, options)`](#updateservicename-options) | `Promise<PublishedService>` | Replace the TXT record of a published service |
| [`resolveService(options)`](#resolveserviceoptions) | `Promise<Service>` | Resolve one service by name, without scanning |
| [`checkLocalNetworkAccess(options?)`](#checklocalnetworkaccessoptions) | `Promise<'granted' \| 'denied' \| 'unknown'>` | Local Network permission (iOS, Android 17) |
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
| `resolveTimeout` | `number` | `5` | all | Seconds to try resolving each service. A timeout is retried once, then reported as an `error` with code `'TIMEOUT'` |
| `subtype` | `string` | none | all | Only find services registered with this subtype, e.g. `'printer'`. See [Subtypes](Scanning#subtypes) |
| `networkInterface` | `string` | all interfaces | all | Interface to scan on, e.g. `'en0'`, `'wlan0'`. Android `NSD` needs Android 13+. See [Choosing a network interface](Scanning#choosing-a-network-interface) |

```javascript
zeroconf.scan()
zeroconf.scan({ type: 'ipp', implType: ImplType.DNSSD, resolveTimeout: 10 })
```

### `stop(implType?)`

Stops this instance's scan and emits `stop`. Scans of other instances keep running.

| Parameter | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `implType` | `'NSD' \| 'DNSSD'` | implementation of the last `scan()` | Android | Which backend to stop |

### `getServices()`

Returns the services of this instance's scan, keyed by name. Services that are found but not yet resolved only contain `name`.

### `scanServiceTypes(options?)`

Starts listing the service types advertised on the network (browsing `_services._dns-sd._udp`), emits `typeFound` and `typeRemove`. Like `scan()`, it replaces this instance's scan, and `stop()` stops it. See [Listing service types](Scanning#listing-service-types) for platform support.

| Option | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `domain` | `string` | `'local.'` | all | Domain to browse |
| `implType` | `'NSD' \| 'DNSSD'` | `'NSD'` | Android | With `NSD`, services that other apps publish on the same phone are left out, this app's own are included. `DNSSD` includes them all |
| `networkInterface` | `string` | all interfaces | all | Interface to list the types on |

### `getServiceTypes()`

Returns the service types found by `scanServiceTypes()`, as `[{ type: 'http', protocol: 'tcp' }]`.

### `publishService(options)`

Advertises a service. Resolves with the service once it is advertised, rejects with a [`ZeroconfError`](Errors). The `published` event is also emitted.

| Option | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `type` | `string` | required | all | Service type without underscore |
| `protocol` | `string` | required | all | `'tcp'` or `'udp'` |
| `domain` | `string` | `'local.'` | all | Domain |
| `name` | `string` | required | all | Requested service name |
| `port` | `number` | required | all | Port |
| `txt` | `object \| [key, value][]` | `{}` | all | TXT record. Pairs keep an explicit order, see [TXT records](Publishing#txt-records) |
| `implType` | `'NSD' \| 'DNSSD'` | `'NSD'` | Android | Backend used to publish |
| `subtypes` | `string[]` | none | all | Subtypes to register, e.g. `['printer']`. See [Subtypes](Publishing#subtypes) |
| `networkInterface` | `string` | all interfaces | all | Interface to publish on. Android `NSD` needs Android 13+ |

> **Use the resolved name.** The published name can differ from `name` when that name is already taken. Pass `service.name` to `unpublishService()`.

### `unpublishService(name, implType?)`

Withdraws a published service. Resolves once it is no longer advertised and emits `unpublished`. Rejects with code `'NOT_PUBLISHED'` if no service is published under `name`.

| Parameter | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `name` | `string` | required | all | Name the service was published as |
| `implType` | `'NSD' \| 'DNSSD'` | backend the service was published with | Android | Which backend to use |

### `checkLocalNetworkAccess(options?)`

Resolves `'granted'`, `'denied'` or `'unknown'`.

- **iOS:** iOS has no API to read the Local Network permission, so this advertises a temporary service and browses for it. It can show the permission prompt if the user has not answered it yet.
- **Android:** on Android 17 (API 37) devices, the `ACCESS_LOCAL_NETWORK` runtime permission is enforced for apps targeting API 37 and for apps declaring it. When it is missing, it is requested (the system prompt shows) unless `request` is `false`. Resolves `'granted'` when there is nothing to grant (earlier versions, or an app not concerned).

| Option | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `type` | `string` | first entry of `NSBonjourServices` | iOS | Service type to test with, must be declared in `NSBonjourServices` |
| `protocol` | `string` | `'tcp'` | iOS | Protocol |
| `timeout` | `number` | `5` | iOS | Seconds before concluding `'denied'` or `'unknown'` |
| `request` | `boolean` | `true` | Android | Request `ACCESS_LOCAL_NETWORK` when it is missing |

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

### `updateService(name, options)`

Replaces the TXT record of a published service and resolves with the updated `PublishedService`. Scanning devices receive the change as a new `resolved` event. Rejects with code `'NOT_PUBLISHED'` for an unknown name. See [Updating the TXT record](Publishing#updating-the-txt-record).

| Option | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `txt` | `TxtRecord` | `{}` | all | The new TXT record |
| `implType` | `'NSD' \| 'DNSSD'` | implementation the service was published with | Android | Android `NSD` publishes the service again under the same name, `DNSSD` and iOS update it in place |

```javascript
await zeroconf.updateService('My Web Server', { txt: { state: 'busy' } })
```

### `resolveService(options)`

Resolves one service by name without scanning, and resolves with a `Service`. Rejects with code `'TIMEOUT'` when the service doesn't answer in time. No events are emitted. See [Resolving a single service](Scanning#resolving-a-single-service).

| Option | Type | Default | Platform | Description |
| --- | --- | --- | --- | --- |
| `name` | `string` | required | all | Service name |
| `type` | `string` | `'http'` | all | Service type without underscore |
| `protocol` | `string` | `'tcp'` | all | `'tcp'` or `'udp'` |
| `domain` | `string` | `'local.'` | all | Domain |
| `implType` | `'NSD' \| 'DNSSD'` | `'NSD'` | Android | Implementation |
| `timeout` | `number` | `5` | all | Seconds before rejecting with `'TIMEOUT'` |
| `networkInterface` | `string` | all interfaces | all | Interface to resolve on |

```javascript
const printer = await zeroconf.resolveService({ name: 'Office Printer', type: 'ipp' })
```

## `useServiceTypes(options?)`

React hook that lists the service types while the component is mounted. Takes the `scanServiceTypes()` options plus `enabled`, and returns `serviceTypes` (`ServiceType[]`) with the same `isScanning`, `error`, `stop()` and `restart()` as `useZeroconf`.

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
| `resolved` | `Service` | A service was resolved. It fires again when its addresses or TXT record change, on iOS, Windows, Android `DNSSD` and Android 14+ with `NSD` (see [Updates](Scanning#updates)) |
| `remove` | `name: string` | A service left the network |
| `update` | none | The list returned by `getServices()` or `getServiceTypes()` changed |
| `typeFound` | `ServiceType` | A service type appeared, during `scanServiceTypes()` |
| `typeRemove` | `ServiceType` | A service type is no longer advertised, during `scanServiceTypes()` |
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

## TypeScript types

Exported from `react-native-zeroconf`:

| Type | Description |
| --- | --- |
| `ImplType` | `'NSD' \| 'DNSSD'` (also a value: `ImplType.NSD`, `ImplType.DNSSD`) |
| `Service` | Resolved service |
| `PublishedService` | `Service` without `ipv4` / `ipv6` |
| `ServiceType` | `{ type: string, protocol: 'tcp' \| 'udp' }` |
| `ScanOptions` | Options of `scan()` |
| `PublishOptions` | Options of `publishService()` |
| `LocalNetworkAccessOptions` | Options of `checkLocalNetworkAccess()` |
| `TxtRecord` | Object or `[key, value]` pairs |
| `ZeroconfError` | Error with `code`, `domain`, `serviceName` |
| `ZeroconfEvents` | Event name to listener signature map |
| `UseZeroconfOptions` | Options of `useZeroconf()` |
| `UseZeroconfResult` | Return value of `useZeroconf()` |
| `UpdateOptions` | Options of `updateService()` |
| `ResolveOptions` | Options of `resolveService()` |
| `ServiceTypesScanOptions` | Options of `scanServiceTypes()` |
| `UseServiceTypesOptions` | Options of `useServiceTypes()` |
| `UseServiceTypesResult` | Return value of `useServiceTypes()` |

Listeners are typed by event name:

```typescript
zeroconf.on('resolved', (service: Service) => {})
zeroconf.on('remove', (name: string) => {})
```
