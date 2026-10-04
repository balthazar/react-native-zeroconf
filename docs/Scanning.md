# Scanning

Find the services other devices advertise on the local network, and read what they resolve to.

- [Scanning for services](#scanning-for-services)
- [Multiple scans](#multiple-scans)
- [Resolved services](#resolved-services)
- [Subtypes](#subtypes)
- [Choosing a network interface](#choosing-a-network-interface)
- [Resolving a single service](#resolving-a-single-service)
- [Listing service types](#listing-service-types)
- [Common service types](#common-service-types)

## Scanning for services

A scan runs in two steps: a service is **found** (only its name is known), then **resolved** (host, port, addresses and TXT records are known).

```javascript
import Zeroconf from 'react-native-zeroconf'

const zeroconf = new Zeroconf()

zeroconf.on('start', () => console.log('Scan started'))
zeroconf.on('found', name => console.log('Found', name))
zeroconf.on('resolved', service => console.log('Resolved', service.name, service.addresses))
zeroconf.on('remove', name => console.log('Gone', name))
zeroconf.on('error', error => console.warn(error.domain, error.code, error.message))

zeroconf.scan({ type: 'http', protocol: 'tcp', domain: 'local.' })
```

All scan options are optional: `scan()` alone browses `_http._tcp.` on `local.`.

- Calling `scan()` again on the same instance replaces that instance's scan and clears its `getServices()`. To browse several types at once, use one instance per scan (see [Multiple scans](#multiple-scans)).
- `stop()` ends the instance's scan. On Android it stops the implementation the last scan used.
- Slow devices can take a while to resolve. Raise `resolveTimeout` (seconds, default `5`); a timed out resolve is retried once before an `error` with code `'TIMEOUT'`.

```javascript
zeroconf.scan({ type: 'ipp', resolveTimeout: 15 })          // slow printers
zeroconf.scan({ type: 'pdl-datastream', implType: 'DNSSD' }) // Android: embedded mDNSResponder
```

### Listing what was found

`getServices()` returns the services of the current scan keyed by name. The `update` event fires whenever that list changes, which makes it a convenient single listener for UIs:

```javascript
zeroconf.on('update', () => {
  const services = Object.values(zeroconf.getServices())
  render(services)
})
```

> Entries that are found but not resolved yet only have a `name`. Check for `port` or `addresses` before using them.
## Multiple scans

Each `Zeroconf` instance runs its own scan, so several scans can run at the same time. Each instance only receives its own scan's events (`start`, `stop`, `found`, `resolved`, `remove`, `update` and scan errors), and `getServices()` only holds its own results.

```javascript
const printers = new Zeroconf()
const speakers = new Zeroconf()

printers.on('resolved', service => console.log('printer', service.name))
speakers.on('resolved', service => console.log('speaker', service.name))

printers.scan({ type: 'ipp' })
speakers.scan({ type: 'raop' })

// Later
printers.stop()
speakers.stop()
```

- An instance that never called `scan()` still receives the events of every scan, as in earlier versions.
- Publishing events (`published`, `unpublished`) and errors not tied to a scan go to every instance.
- On Android, `NSD` and `DNSSD` scans can run at the same time, for example `printers.scan({ type: 'ipp', implType: 'DNSSD' })` next to an `NSD` scan.
- On iOS, every scanned type must be listed in `NSBonjourServices` (here `_ipp._tcp` and `_raop._tcp`).
- With the [hook](React-Integration#the-usezeroconf-hook), each `useZeroconf` call runs its own scan, so several components can scan different types at once.
## Resolved services

```javascript
{
  name: 'Xerox Printer',
  fullName: 'XeroxPrinter._http._tcp.local.',
  host: 'XeroxPrinter.local.',
  port: 8080,
  addresses: ['192.168.1.23', 'fe80::aebc:123:ffff:abcd'], // IPv4 first
  ipv4: ['192.168.1.23'],
  ipv6: ['fe80::aebc:123:ffff:abcd'],
  txt: { path: '/status', color: 'yes' },
}
```

To connect, prefer `service.ipv4[0]` (or `service.addresses[0]`, which is IPv4 when one exists):

```javascript
zeroconf.on('resolved', service => {
  const address = service.ipv4[0] ?? service.addresses[0]
  if (address) fetch(`http://${address}:${service.port}${service.txt.path ?? '/'}`)
})
```

### Updates

A resolved service is emitted as `resolved` again when its addresses or TXT record change, so update your list by `service.name` rather than appending:

| Platform | `resolved` again on changes |
| --- | --- |
| iOS | Yes |
| Android `NSD` | Android 14+ |
| Android `DNSSD` | Yes |
| Windows | Yes, when the TXT record or the host and port change |

> On Android with `NSD`, `host` is the mDNS host name only on Android 16+. Earlier versions usually give the IP address in `host`. Use `implType: 'DNSSD'` if you need the host name there. See [Android Implementations](Android-Implementations).
## Subtypes

Services can be registered with subtypes (`_printer._sub._ipp._tcp`), for example to tell color printers apart. Scan a subtype to only find those services:

```javascript
zeroconf.scan({ type: 'ipp', subtype: 'printer' })
```

`subtype` takes the label with or without the leading underscore. Publish with subtypes using `subtypes: ['printer']` (see [Publishing](Publishing#subtypes)). Subtypes work on iOS and with both Android implementations.

## Choosing a network interface

By default, scans use every network interface. Pass `networkInterface` to use one, by its system name:

```javascript
zeroconf.scan({ type: 'http', networkInterface: Platform.OS === 'ios' ? 'en0' : 'wlan0' })
```

- Common names: `en0` (iOS Wi-Fi), `wlan0` (Android Wi-Fi), `eth0` (Android Ethernet).
- An interface that doesn't exist emits an `error` with code `'UNKNOWN_INTERFACE'`.
- Android `NSD` needs Android 13 or later for it, earlier versions emit an `error` with code `'UNSUPPORTED'`. `DNSSD` supports every version.

`publishService()` and `resolveService()` take the same option.

## Resolving a single service

To reach a service you found before, resolve it by name instead of scanning again:

```javascript
try {
  const printer = await zeroconf.resolveService({ name: 'Office Printer', type: 'ipp' })
  console.log(printer.ipv4[0], printer.port)
} catch (error) {
  if (error.code === 'TIMEOUT') {
    // Not on the network right now
  }
}
```

It resolves once with a `Service` and doesn't emit events. `timeout` (seconds, default `5`) sets how long to wait before rejecting with code `'TIMEOUT'`.

## Listing service types

mDNS only finds services of a type you ask for. To see which types are advertised on the network, list them first:

```javascript
const zeroconf = new Zeroconf()

zeroconf.on('typeFound', ({ type, protocol }) => console.log(`_${type}._${protocol}`))
zeroconf.on('typeRemove', ({ type, protocol }) => console.log(`gone: _${type}._${protocol}`))
zeroconf.scanServiceTypes()

// Later
zeroconf.getServiceTypes() // [{ type: 'http', protocol: 'tcp' }, { type: 'ipp', protocol: 'tcp' }]
```

Then scan each type you are interested in, with one instance per type or `useZeroconf` per type (see [Multiple scans](#multiple-scans)). Service types are not resolved, `found` and `resolved` are not emitted while listing them. In React, use [`useServiceTypes`](React-Integration#listing-service-types).

| Platform | Support |
| --- | --- |
| Android | Yes. With `NSD` (the default), services that other apps publish on the same phone are left out, this app's own are included. `DNSSD` includes them all |
| Windows | Yes. Services that other apps publish on the same computer are left out, this app's own are included |
| iOS | Requires the multicast entitlement (`com.apple.developer.networking.multicast`), which apps request from Apple. Without it, listing fails with error `-65555`. Apple's [Local Network Privacy FAQ](https://developer.apple.com/forums/thread/663875) lists browsing for service types among the operations that need it. Apps without the entitlement scan a known list of types, declared in `NSBonjourServices` |

On Android, `NsdManager` can't list service types (Android 14 and later read `_services._dns-sd._udp` as a subtype), so with `NSD` the library queries the network itself: it sends the mDNS query from its own socket and repeats it every 10 seconds, a type is removed after 35 seconds without an answer.

## Common service types

| Service | `type` | `protocol` |
| --- | --- | --- |
| HTTP | `http` | `tcp` |
| HTTPS | `https` | `tcp` |
| Printer (raw) | `pdl-datastream` | `tcp` |
| Printer (IPP) | `ipp` | `tcp` |
| SSH | `ssh` | `tcp` |
| FTP | `ftp` | `tcp` |
| AirPlay | `airplay` | `tcp` |
| Chromecast | `googlecast` | `tcp` |

Remember: on iOS each type you use must be in `NSBonjourServices` (for example `_ipp._tcp`).

---

Next: [Publishing](Publishing)
