# React Integration

Scan and publish from React components, and clean up when they unmount.

- [The `useZeroconf` hook](#the-usezeroconf-hook)
- [Listing service types](#listing-service-types)
- [Cleanup in React components](#cleanup-in-react-components)
- [Recipe: publish while a screen is mounted](#recipe-publish-while-a-screen-is-mounted)

## The `useZeroconf` hook

The easiest way to scan from a component. It scans while mounted, scans again when the options change, and stops and cleans up on unmount.

```tsx
import { useZeroconf } from 'react-native-zeroconf'

function Printers() {
  const { services, isScanning, error, restart } = useZeroconf({ type: 'ipp' })

  return (
    <FlatList
      data={services}
      keyExtractor={service => service.name}
      ListHeaderComponent={error ? <Text>{error.message}</Text> : null}
      renderItem={({ item }) => <Text>{item.name}: {item.ipv4[0]}:{item.port}</Text>}
      refreshing={isScanning}
      onRefresh={restart}
    />
  )
}
```

Pass `enabled: false` to scan only when needed, for example while a screen is focused:

```tsx
const isFocused = useIsFocused()
const { services } = useZeroconf({ type: 'http', enabled: isFocused })
```

Each hook runs its own scan, so several hooks can scan different types at the same time:

```tsx
function Devices() {
  const printers = useZeroconf({ type: 'ipp' })
  const speakers = useZeroconf({ type: 'raop' })

  return <DeviceList printers={printers.services} speakers={speakers.services} />
}
```
## Listing service types

`useServiceTypes` lists the service types on the network, and each type can be scanned by its own component:

```tsx
import { useServiceTypes, useZeroconf } from 'react-native-zeroconf'

function Network() {
  const { serviceTypes } = useServiceTypes()
  return serviceTypes.map(t => <ServicesOfType key={`${t.type}.${t.protocol}`} {...t} />)
}

function ServicesOfType({ type, protocol }) {
  const { services } = useZeroconf({ type, protocol })
  return services.map(service => <Text key={service.name}>{service.name} ({type})</Text>)
}
```

Listing service types works on Android, and is restricted on iOS: see [Listing service types](Scanning#listing-service-types).

## Cleanup in React components

When you use a `Zeroconf` instance directly, clean up when the component unmounts:

1. `stop()` the scan it started,
2. unpublish the services it published,
3. call `removeDeviceListeners()` so the instance stops receiving native events.

`subscribe()` returns a function that removes the listener, which fits `useEffect` cleanups:

```javascript
useEffect(() => {
  const zeroconf = new Zeroconf()
  const unsubscribe = zeroconf.subscribe('resolved', service => console.log(service.name))
  zeroconf.scan({ type: 'http' })

  return () => {
    unsubscribe()
    zeroconf.stop()
    zeroconf.removeDeviceListeners()
  }
}, [])
```
## Recipe: publish while a screen is mounted

```typescript
useEffect(() => {
  const zeroconf = new Zeroconf()
  let publishedName: string | null = null
  let cancelled = false

  zeroconf
    .publishService({ type: 'http', protocol: 'tcp', name: 'My Device', port: 8080 })
    .then(service => {
      publishedName = service.name
      if (cancelled) zeroconf.unpublishService(service.name).catch(() => {})
    })
    .catch(error => console.warn(error.code, error.message))

  return () => {
    cancelled = true
    if (publishedName) zeroconf.unpublishService(publishedName).catch(() => {})
    zeroconf.removeDeviceListeners()
  }
}, [])
```

---

Next: [Error Handling](Error-Handling)
