# Publishing

Advertise your own services on the local network, with their TXT records.

- [Publishing a service](#publishing-a-service)
- [TXT records](#txt-records)

## Publishing a service

```javascript
try {
  const service = await zeroconf.publishService({
    type: 'http',
    protocol: 'tcp',
    name: 'My Web Server',
    port: 8080,
    txt: { path: '/api', version: '1.0' },
  })
  console.log(`Advertised as ${service.name}`)
} catch (error) {
  console.warn(error.domain, error.code, error.message)
}
```

- The promise resolves once the service is advertised. The `published` event fires too.
- **The name can change.** If the name is already taken, another one is picked (on iOS, for example, `My Web Server (2)`). Always keep the resolved `service.name`.
- Unpublish with that name. The promise resolves once the service is withdrawn and rejects with code `'NOT_PUBLISHED'` if nothing is published under that name.

```javascript
await zeroconf.unpublishService(service.name)
```

- Published services are unpublished automatically when the React Native instance is torn down (for example on reload). If the process is killed, nothing can be announced: other devices drop the service when its mDNS records expire.
- On iOS, remember to list the published type in `NSBonjourServices` too.
## TXT records

Pass an object, or an array of `[key, value]` pairs when order matters:

```javascript
// Object: published in insertion order
txt: { txtvers: '1', path: '/api' }

// Pairs: explicit order, safe for keys that look like numbers
txt: [
  ['txtvers', '1'],
  ['0', 'first'],
]
```

> JavaScript objects move integer-like keys (`'0'`, `'1'`...) to the front. Use pairs if a protocol requires a specific order with such keys.

| Where | Order preserved? |
| --- | --- |
| iOS | Yes |
| Android `DNSSD` | Yes |
| Android `NSD` | Depends on the Android system |

Values are converted to strings. On iOS, a `key=value` entry longer than 255 bytes is left out of the record and reported through an `error` event with code `'TXT_ENTRY_TOO_LONG'` (the service is still published).

Received TXT records are exposed as `service.txt`, an object of strings.

---

Next: [React Integration](React-Integration)
