# react-native-zeroconf

> Zeroconf (Bonjour/mDNS) implementation for React Native

Discover and publish network services using Zeroconf protocols (Bonjour, Avahi, mDNS).

## Features

- **Service Discovery**: Find services advertised on your local network
- **Service Publishing**: Advertise your own services
- **Cross-Platform**: Works on iOS and Android
- **Dual Android Implementation**: Choose between NSD (Android native) or DNSSD (embedded mDNSResponder)
- **Android 15+ Compatible**: Includes 16KB page size alignment (Google Play requirement starting November 1, 2025)

## Installation

```bash
# Install using yarn
yarn add react-native-zeroconf

# For React Native < 0.60 only (all platforms):
react-native link

# For iOS (using CocoaPods):
cd ios && pod install

# For macOS (using CocoaPods):
cd macos && pod install
```

For manual installation, see the [wiki](https://github.com/balthazar/react-native-zeroconf/wiki).

## Setup

### Android Permissions

Add the following permissions to your `AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
```

### iOS 14+ Permissions

iOS 14+ requires you to declare the services you want to discover in your `Info.plist`:

```xml
<key>NSBonjourServices</key>
<array>
    <string>_http._tcp.</string>
    <string>_printer._tcp.</string>
    <!-- Add other service types you need -->
</array>
<key>NSLocalNetworkUsageDescription</key>
<string>This app uses the local network to discover printers and other devices.</string>
```

## Quick Start

```javascript
import Zeroconf from 'react-native-zeroconf'

const zeroconf = new Zeroconf()

// Listen for resolved services
zeroconf.on('resolved', service => {
  console.log('Found service:', service.name)
  console.log('IP addresses:', service.addresses)
  console.log('Port:', service.port)
})

// Start scanning for HTTP services
zeroconf.scan('http', 'tcp', 'local.')

// Stop scanning after 10 seconds
setTimeout(() => {
  zeroconf.stop()
  console.log('All services:', zeroconf.getServices())
}, 10000)
```

## API Reference

### Constructor

```javascript
import Zeroconf from 'react-native-zeroconf'
const zeroconf = new Zeroconf()
```

### Methods

#### `scan(type, protocol, domain, implType)`

Start scanning for services on the network.

| Parameter  | Type   | Default    | Description                                                                                        |
| ---------- | ------ | ---------- | -------------------------------------------------------------------------------------------------- |
| `type`     | string | `'http'`   | Service type (e.g., `'http'`, `'printer'`, `'ssh'`, `'pdl-datastream'`)                            |
| `protocol` | string | `'tcp'`    | Protocol (`'tcp'` or `'udp'`)                                                                      |
| `domain`   | string | `'local.'` | Domain to search (typically `'local.'`)                                                            |
| `implType` | string | `'NSD'`    | **Android only**: `'NSD'` or `'DNSSD'` (see [Implementation Types](#android-implementation-types)) |

```javascript
// Scan for HTTP services using default NSD implementation
zeroconf.scan('http', 'tcp', 'local.')

// Scan for printers using DNSSD (recommended for better compatibility)
zeroconf.scan('pdl-datastream', 'tcp', 'local.', 'DNSSD')
```

#### `stop(implType)`

Stop the current scan.

| Parameter  | Type   | Default | Description                                    |
| ---------- | ------ | ------- | ---------------------------------------------- |
| `implType` | string | `'NSD'` | **Android only**: Which implementation to stop |

```javascript
zeroconf.stop()
// or on Android with DNSSD:
zeroconf.stop('DNSSD')
```

#### `getServices()`

Returns all currently discovered services.

```javascript
const services = zeroconf.getServices()
// Returns: { 'ServiceName': { name, host, port, addresses, txt, fullName }, ... }
```

#### `publishService(type, protocol, domain, name, port, txt, implType)`

Publish a service on the network.

| Parameter  | Type   | Default    | Description                            |
| ---------- | ------ | ---------- | -------------------------------------- |
| `type`     | string | required   | Service type (e.g., `'http'`)          |
| `protocol` | string | required   | Protocol (`'tcp'` or `'udp'`)          |
| `domain`   | string | `'local.'` | Domain                                 |
| `name`     | string | required   | Service name (should be unique)        |
| `port`     | number | required   | Port number                            |
| `txt`      | object | `{}`       | TXT record key-value pairs             |
| `implType` | string | `'NSD'`    | **Android only**: `'NSD'` or `'DNSSD'` |

```javascript
zeroconf.publishService('http', 'tcp', 'local.', 'MyWebServer', 8080, {
  path: '/api',
  version: '1.0',
})
```

#### `unpublishService(name, implType)`

Remove a published service.

| Parameter  | Type   | Default  | Description                            |
| ---------- | ------ | -------- | -------------------------------------- |
| `name`     | string | required | Name of the service to unpublish       |
| `implType` | string | `'NSD'`  | **Android only**: Which implementation |

```javascript
zeroconf.unpublishService('MyWebServer')
```

#### `addDeviceListeners()`

Manually add event listeners (called automatically in constructor).

#### `removeDeviceListeners()`

Remove all event listeners. Call this to prevent memory leaks when unmounting components.

```javascript
// In React useEffect cleanup
useEffect(() => {
  const zeroconf = new Zeroconf()
  zeroconf.scan('http', 'tcp', 'local.')

  return () => {
    zeroconf.stop()
    zeroconf.removeDeviceListeners()
  }
}, [])
```

### Events

#### Scan Events

| Event      | Payload                 | Description                              |
| ---------- | ----------------------- | ---------------------------------------- |
| `start`    | none                    | Scan has started                         |
| `stop`     | none                    | Scan has stopped                         |
| `found`    | `string` (service name) | Service found (before resolution)        |
| `resolved` | `Service` object        | Service fully resolved with network info |
| `remove`   | `string` (service name) | Service removed from network             |
| `update`   | none                    | Services list changed (found or removed) |
| `error`    | `Error` object          | An error occurred                        |

#### Publishing Events

| Event         | Payload          | Description                      |
| ------------- | ---------------- | -------------------------------- |
| `published`   | `Service` object | Service successfully published   |
| `unpublished` | `Service` object | Service successfully unpublished |

### Service Object

```javascript
{
  name: 'Xerox Printer',                    // Human-readable name
  fullName: 'XeroxPrinter._http._tcp.local.', // Full service name
  host: 'XeroxPrinter.local.',              // Hostname
  port: 8080,                               // Port number
  addresses: [                              // IP addresses (IPv4 and/or IPv6)
    '192.168.1.23',
    'fe80::aebc:123:ffff:abcd'
  ],
  txt: {                                    // TXT record attributes
    path: '/status',
    color: 'yes'
  }
}
```

## Android Implementation Types

This library supports two implementations on Android:

### NSD (Network Service Discovery)

- Uses Android's built-in `NsdManager` API
- Default implementation
- Good for most use cases

### DNSSD (Embedded mDNSResponder)

- Uses bundled Apple mDNSResponder
- More reliable across different Android versions and manufacturers
- Required for 16KB page size compliance (Android 15+)
- Recommended for production apps

```javascript
import Zeroconf, { ImplType } from 'react-native-zeroconf'

const zeroconf = new Zeroconf()

// Use DNSSD for better compatibility
zeroconf.scan('http', 'tcp', 'local.', ImplType.DNSSD)
```

### When to Use DNSSD

- **Targeting Android 15+**: Google Play requires 16KB page size alignment
- **Cross-device compatibility**: More consistent behavior across manufacturers
- **Printer discovery**: Better support for `_pdl-datastream._tcp` and similar services
- **When NSD doesn't find services**: Some devices have buggy NSD implementations

## Platform Support

| Feature            | iOS    | Android (NSD) | Android (DNSSD) |
| ------------------ | ------ | ------------- | --------------- |
| Service Discovery  | ✅     | ✅            | ✅              |
| Service Publishing | ✅     | ✅            | ✅              |
| TXT Records        | ✅     | ✅ (API 21+)  | ✅              |
| IPv4 Addresses     | ✅     | ✅            | ✅              |
| IPv6 Addresses     | ✅     | ✅            | ✅              |
| Min API Level      | iOS 7+ | API 16+       | API 21+         |

## Android Emulator Limitations

**Important:** The Android emulator does not support IGMP or multicast by default. This is a [documented limitation](https://developer.android.com/studio/run/emulator-networking).

Since mDNS/Bonjour relies on multicast UDP packets to `224.0.0.251:5353`, Zeroconf discovery **will not work on Android emulators** without special configuration.

### Recommendations

**Use a Real Device (Recommended)**

For reliable mDNS testing, use a physical Android device connected to the same network as the services you want to discover.

**TAP Bridged Networking (Linux - Really Advanced - Not Recommended for Most Users)**

> **⚠️ Important:**
>
> - **Use "Google APIs" system image**, not "Google Play". Google Play images are production builds that don't allow root access (`adb root` fails). You need root to configure the `eth1` interface.
> - **Always use `-no-snapshot-load`** when starting the emulator. Without this flag, the emulator loads from a saved state and ignores the QEMU network device parameters (eth1 won't exist).
> - **Network configuration doesn't persist.** You must reconfigure eth1 after every emulator restart.
> - **Only one emulator per TAP interface.** If running multiple emulators, create additional TAP interfaces (tap1, tap2, etc.).
> - **Use your network's subnet.** Replace the example IPs below with addresses matching your actual network (e.g., if your network is `192.168.1.x`, use `192.168.1.213/24` and gateway `192.168.1.1`).

On Linux with Ethernet, you can configure TAP bridged networking:

1. Create TAP interface and bridge (one-time setup):

   ```bash
   sudo ip tuntap add dev tap0 mode tap user $USER
   sudo ip link add name br0 type bridge
   sudo ip link show # Get the list of network interfaces
   sudo ip link set <your-ethernet-interface> master br0  # Replace with your ethernet interface (e.g. enp3s0)
   sudo ip link set tap0 master br0
   sudo ip link set dev tap0 up
   sudo ip link set dev br0 up
   sudo dhcpcd br0  # Or: sudo dhclient br0
   ```

2. Start emulator with TAP (must use `-no-snapshot-load`):

   ```bash
   emulator -avd <avd_name> \
     -no-snapshot-load \
     -qemu \
     -netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no \
     -device virtio-net-pci,netdev=mynet0
   ```

3. Configure the emulator's eth1 interface:

   ```bash
   adb root
   adb shell ip link set eth1 up
   adb shell ip addr add <unused_ip> dev eth1   # e.g., 192.168.1.213
   adb shell ip route add default via <gateway> dev eth1  # e.g., 192.168.1.1

   # Verify connectivity
   adb shell ping -c 2 <your_printer_or_device_ip>
   ```

4. Start your dev server with the host's bridge IP:

   ```bash
   # Find your host's bridge IP
   ip addr show br0 | grep "inet "
   # Example output: inet 192.168.1.28/24 ...

   # Example to use that IP to start with Expo
   REACT_NATIVE_PACKAGER_HOSTNAME=192.168.1.28 npx expo start --android
   ```

**Note:** WiFi bridging doesn't work on most systems. Use Ethernet for TAP networking.

## Common Service Types

| Service       | Type             | Protocol |
| ------------- | ---------------- | -------- |
| HTTP          | `http`           | `tcp`    |
| HTTPS         | `https`          | `tcp`    |
| Printer (Raw) | `pdl-datastream` | `tcp`    |
| Printer (IPP) | `ipp`            | `tcp`    |
| SSH           | `ssh`            | `tcp`    |
| FTP           | `ftp`            | `tcp`    |
| AirPlay       | `airplay`        | `tcp`    |
| Chromecast    | `googlecast`     | `tcp`    |

## Example

See the [example folder](./example) for a complete React Native app demonstrating service discovery and publishing.

```bash
cd example
yarn install
cd ios && pod install && cd ..
yarn ios  # or yarn android
```

## Troubleshooting

### Services not being discovered

1. **Check permissions**: Ensure all required permissions are granted
2. **iOS 14+**: Verify `NSBonjourServices` includes your service type
3. **Android emulator**: Use a real device (emulators don't support multicast)
4. **Try DNSSD**: Switch from NSD to DNSSD implementation on Android
5. **Same network**: Ensure device and services are on the same network/subnet

### TXT records empty or missing

- TXT records require Android API 21+ (Android 5.0)
- Verify the service actually publishes TXT records

### Memory leaks

Call `removeDeviceListeners()` when unmounting:

```javascript
useEffect(() => {
  const zeroconf = new Zeroconf()
  // ... setup
  return () => {
    zeroconf.stop()
    zeroconf.removeDeviceListeners()
  }
}, [])
```

## Known Issues

### Android mDNS/NSD Reliability

Android's Network Service Discovery (NSD) implementation has well-documented reliability issues that affect **all** mDNS libraries on Android, including this one. These are platform-level limitations, not bugs in this library.

#### Symptoms

- Discovery works initially, then stops finding services after a few scans
- `start` event fires but no `resolved` events follow
- Inconsistent results between scans on the same network
- Discovery fails silently without error callbacks

#### Root Causes

1. **Android NSD limitations (Android 8–14+)**:
   - Discovery may silently stop without error callbacks
   - `onServiceFound` fires but resolve fails
   - Discovery stops after screen lock or app backgrounding
   - Multiple concurrent scans conflict with each other

2. **OEM-specific issues** (Samsung, Xiaomi, Huawei, etc.):
   - Multicast packets throttled or dropped
   - Background discovery threads killed by battery optimization
   - Aggressive Doze mode delays mDNS packets

3. **Network change sensitivity**:
   - mDNS breaks on WiFi reconnect, AP band switching (2.4↔5 GHz), mesh handoff, or VPN changes

#### Recommended Workarounds

**1. Implement retry logic**

Since Android NSD fails silently, implement automatic retries:

```javascript
async function scanWithRetry(zeroconf, maxAttempts = 5) {
  for (let attempt = 1; attempt <= maxAttempts; attempt++) {
    const results = await performScan(zeroconf)
    if (results.length > 0) return results

    // Stop and wait before retry
    zeroconf.stop('DNSSD')
    await new Promise(r => setTimeout(r, 1000))
  }
  return []
}
```

**2. Add delays between stop and start**

The native DNSSD module needs time to fully stop before starting a new scan:

```javascript
zeroconf.stop('DNSSD')
await new Promise(r => setTimeout(r, 500)) // Wait 500ms
zeroconf.scan('pdl-datastream', 'tcp', 'local.', 'DNSSD')
```

**3. Use DNSSD instead of NSD**

DNSSD (embedded mDNSResponder) is generally more reliable than Android's native NSD:

```javascript
// Use DNSSD for better reliability
zeroconf.scan('http', 'tcp', 'local.', 'DNSSD')
```

**4. Handle app lifecycle**

Stop scans when app backgrounds and restart when foregrounding:

```javascript
import { AppState } from 'react-native'

AppState.addEventListener('change', (state) => {
  if (state === 'background') {
    zeroconf.stop('DNSSD')
  } else if (state === 'active') {
    // Restart scan
    zeroconf.scan('http', 'tcp', 'local.', 'DNSSD')
  }
})
```

**5. Don't call stop() on already-stopped scans**

Track scan state to avoid calling `stop()` multiple times, which can put the native module in a bad state:

```javascript
let isScanning = false

function startScan() {
  if (isScanning) return
  isScanning = true
  zeroconf.scan('http', 'tcp', 'local.', 'DNSSD')
}

function stopScan() {
  if (!isScanning) return
  isScanning = false
  zeroconf.stop('DNSSD')
}
```

#### What Doesn't Help

- Creating multiple Zeroconf instances (native module is a singleton)
- Calling `removeDeviceListeners()` + `addDeviceListeners()` rapidly
- Very short scan timeouts (< 3 seconds)

#### References

- [Android NsdManager documentation](https://developer.android.com/reference/android/net/nsd/NsdManager)
- [Android emulator multicast limitations](https://developer.android.com/studio/run/emulator-networking)

## About

The library [react-native-zeroconf](https://github.com/balthazar/react-native-zeroconf) includes:

- **16KB Page Size Support**: Native libraries built with 16KB alignment for Android 15+ (Google Play requirement as of November 1, 2025)
- **Bundled RxDNSSD**: Native DNS-SD code from [Discord's RxDNSSD fork](https://github.com/discord/RxDNSSD) is embedded directly
- **Embedded mDNSResponder**: Works reliably across all Android versions without depending on system daemons
- **Zero Security Vulnerabilities**: All dependencies updated to modern versions

## License

MIT License - see [LICENSE](LICENSE) file.

### Third-Party Licenses

This project includes code from [RxDNSSD](https://github.com/discord/RxDNSSD) (originally by Andriy Druk, maintained by Discord), licensed under the Apache License 2.0. See [NOTICE](NOTICE) file.
